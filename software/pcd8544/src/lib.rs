
extern crate sysfs_gpio;
extern crate spidev;

mod font;
mod terminus6x12;

use sysfs_gpio::{Direction, Pin};
use spidev::{Spidev, SpidevOptions, SPI_MODE_0};
use std::io::Write;
use std::thread::sleep;
use std::time::Duration;

// Port of Adafruit's Nokia LCD library in Python.

const LCDWIDTH  : usize = 84;
const LCDHEIGHT : usize = 48;
const ROWPIXELS : usize = LCDHEIGHT / 6;
const BUFFER_LEN : usize = LCDWIDTH * LCDHEIGHT / 8;
const DEFAULT_CONTRAST : u8 = 40;
const DEFAULT_BIAS     : u8 = 4;

const PCD8544_POWERDOWN           : u8 = 0x04;
const PCD8544_ENTRYMODE           : u8 = 0x02;
const PCD8544_EXTENDEDINSTRUCTION : u8 = 0x01;
const PCD8544_DISPLAYBLANK        : u8 = 0x00;
const PCD8544_DISPLAYNORMAL       : u8 = 0x04;
const PCD8544_DISPLAYALLON        : u8 = 0x01;
const PCD8544_DISPLAYINVERTED     : u8 = 0x05;
const PCD8544_FUNCTIONSET         : u8 = 0x20;
const PCD8544_DISPLAYCONTROL      : u8 = 0x08;
const PCD8544_SETYADDR            : u8 = 0x40;
const PCD8544_SETXADDR            : u8 = 0x80;
const PCD8544_SETTEMP             : u8 = 0x04;
const PCD8544_SETBIAS             : u8 = 0x10;
const PCD8544_SETVOP              : u8 = 0x80;

pub enum Rotation {
    None,
    Left,
    Right,
    UpsideDown
}

pub struct PCD8544 {
    dc : Pin,
    rst : Pin,
    spi : Spidev,
    buffer : [u8 ; BUFFER_LEN],
    rotation : Rotation
}

#[derive(Debug)]
pub enum Error {
    PinError(sysfs_gpio::Error),
    SpiDevError(std::io::Error)
}

impl From<sysfs_gpio::Error> for Error {
    fn from(e : sysfs_gpio::Error) -> Error {
        Error::PinError(e)
    }
}

impl From<std::io::Error> for Error {
    fn from(e : std::io::Error) -> Error {
        Error::SpiDevError(e)
    }
}

type Result<T> = std::result::Result<T, Error>;

fn new_pin(n : u64, dir : Direction, timeout : Duration, retries : u32) -> Result<Pin> {
    let pin = Pin::new(n);

    // Assume the pin will be correctly configured.
    let mut res : Result<Pin> = Ok(pin);

    // Export the sysfs entry for the chosen pin.
    pin.export()?;

    // The sysfs entry might not be immediately usable
    // after the export operation.
    // We will call set_direction() repeatedly until the operation completes
    // or after a given number of attempts.
    for k in 0..retries {
        if k > 0 {
            sleep(timeout);
        }
        match pin.set_direction(dir) {
            Ok(_)  => return Ok(pin),
            Err(e) => res = Err(Error::from(e))
        }
    }

    // Return the last result.
    res
}

impl PCD8544 {
    pub fn new(dc : u64, rst : u64, spi : &str, rot : Rotation) -> Result<Self> {
        let mut spidev = Spidev::open(spi)?;
        let mut options = SpidevOptions::new();
        options.bits_per_word(8).max_speed_hz(4_000_000).mode(SPI_MODE_0);
        spidev.configure(&options)?;

        let mut res = Self {
            dc  : new_pin(dc,  Direction::Out, Duration::from_millis(100), 3)?,
            rst : new_pin(rst, Direction::Out, Duration::from_millis(100), 3)?,
            spi : spidev,
            buffer : [0x00 ; BUFFER_LEN],
            rotation : rot
        };

        res.reset()?;
        res.set_contrast(DEFAULT_CONTRAST)?;
        res.set_bias(DEFAULT_BIAS)?;

        Ok(res)
    }

    pub fn reset(&mut self) -> Result<()> {
        self.rst.set_value(0)?;
        sleep(Duration::from_millis(100));
        self.rst.set_value(1)?;
        Ok(())
    }

    pub fn send_command(&mut self, c : u8) -> Result<()> {
        self.dc.set_value(0)?;
        self.spi.write(&[c])?;
        Ok(())
    }

    pub fn send_extended_command(&mut self, c : u8) -> Result<()> {
        // Set extended command mode
        self.send_command(PCD8544_FUNCTIONSET | PCD8544_EXTENDEDINSTRUCTION)?;
        self.send_command(c)?;
        // Set normal display mode.
        self.send_command(PCD8544_FUNCTIONSET)?;
        self.send_command(PCD8544_DISPLAYCONTROL | PCD8544_DISPLAYNORMAL)?;
        Ok(())
    }

    pub fn send_data_byte(&mut self, c : u8) -> Result<()> {
        self.dc.set_value(1)?;
        self.spi.write(&[c])?;
        Ok(())
    }

    pub fn set_contrast(&mut self, contrast : u8) -> Result<()> {
        let mut c = contrast;
        if c > 127 {
            c = 127;
        }
        self.send_extended_command(PCD8544_SETVOP | c)?;
        Ok(())
    }

    pub fn set_bias(&mut self, bias : u8) -> Result<()> {
        self.send_extended_command(PCD8544_SETBIAS | bias)?;
        Ok(())
    }

    pub fn update(&mut self) -> Result<()> {
        // TODO: Consider support for partial updates like Arduino library.
        // Reset to position zero.
        self.send_command(PCD8544_SETYADDR)?;
        self.send_command(PCD8544_SETXADDR)?;
        // Write the buffer.
        self.dc.set_value(1)?;
        self.spi.write(&self.buffer)?;
        Ok(())
    }

    pub fn clear(&mut self) {
        self.buffer = [0x00 ; BUFFER_LEN]
    }

    pub fn set_pixel(&mut self, x : usize, y : usize, value : bool) {
        let (px, py) = match self.rotation {
            Rotation::None       => (x, y),
            Rotation::Left       => (LCDWIDTH - 1 - y, x),
            Rotation::Right      => (y, LCDHEIGHT - 1 - x),
            Rotation::UpsideDown => (LCDWIDTH - 1 - x, LCDHEIGHT - 1 - y)
        };

        if px >= LCDWIDTH || py >= LCDHEIGHT {
            return
        }

        let bv : u8 = 1 << (y % 8);

        if value {
            self.buffer[x + (y / 8) * LCDWIDTH] |= bv;
        }
        else {
            self.buffer[x + (y / 8) * LCDWIDTH] &= !bv;
        }
    }

    pub fn print_char(&mut self, x : usize, y : usize, c : char) {
        // Get the index of the current character in the font.
        let index = match terminus6x12::ENCODING.iter().position(|&v| v == c as u16) {
            Some(k) => k,
            None    => 0xFFFD
        };

        // Compute the index range in the font bitmap array.
        let start = index * terminus6x12::HEIGHT;
        let end   = start + terminus6x12::HEIGHT - 1;

        // Convert character coordinates to pixels.
        let xp = x * terminus6x12::WIDTH;
        let yp = y * terminus6x12::HEIGHT;

        for r in start..end {
            let b = terminus6x12::BITMAP[r];
            let mut m = 0x80;
            for k in 0..7 {
                self.set_pixel(xp + k, yp + r, b & m != 0x00);
                m >>= 1;
            }
        }
    }

    pub fn print(&mut self, x : usize, y : usize, s : &str) {
        let mut xc = x;
        let mut yc = y;
        for c in s.chars() {
            self.print_char(xc, yc, c);
            xc += 1;
            if xc * terminus6x12::WIDTH >= LCDWIDTH {
                xc = 0;
                yc += 1;
                if yc * terminus6x12::HEIGHT >= LCDHEIGHT {
                    break;
                }
            }
        }
    }
}
/*

    def image(self, image):
        """Set buffer to value of Python Imaging Library image.  The image should
        be in 1 bit mode and have a size of 84x48 pixels."""
        if image.mode != '1':
            raise ValueError('Image must be in mode 1.')
        index = 0
        // Iterate through the 6 y axis rows.
        // Grab all the pixels from the image, faster than getpixel.
        pix = image.load()
        for row in range(6):
            // Iterate through all 83 x axis columns.
            for x in range(84):
                // Set the bits for the column of pixels at the current position.
                bits = 0
                // Don't use range here as it's a bit slow
                for bit in [0, 1, 2, 3, 4, 5, 6, 7]:
                    bits = bits << 1
                    bits |= 1 if pix[(x, row*ROWPIXELS+7-bit)] == 0 else 0
                // Update buffer byte and increment to next byte.
                self._buffer[index] = bits
                index += 1
*/
