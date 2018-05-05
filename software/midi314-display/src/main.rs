
extern crate jack;
extern crate midi314;
extern crate pcd8544;

use std::{thread, time};
use midi314::{Keyboard, LoopManager, LoopState};
use pcd8544::{PCD8544, Rotation};

const LCD_RST : u64 = 24;
const LCD_DC  : u64 = 25;
const LCD_SPI : &'static str = "/dev/spidev0.0";
const LCD_ROT : Rotation = Rotation::None;

struct Display {
    loop_states : Vec<LoopState>,
    lcd : Option<PCD8544>
}

impl Display {
    fn new(n : usize) -> Self {
        Self {
            loop_states : vec![LoopState::Empty ; n],
            lcd : PCD8544::new(LCD_DC, LCD_RST, LCD_SPI, LCD_ROT).ok()
        }
    }

    fn show(&mut self, kb : &Keyboard) {
        if self.lcd.is_some() {
            let lcd = self.lcd.as_mut().unwrap();
            lcd.print(0, 0, &format!("Keys {} - {}", kb.get_min_note_name(), kb.get_max_note_name()));
            lcd.print(0, 1, &format!("Prog {} - {}", kb.min_program + 1, kb.min_program + kb.program_keys));
            if kb.percussion {
                lcd.print(0, 2, "Curr [Percussion]")
            }
            else {
                // TODO map current program to instrument name.
                lcd.print(0, 2, &format!("Curr {}", kb.current_program + 1))
            }
            lcd.print(0, 3, "Loop");
            for (i, l) in (&self.loop_states).iter().enumerate() {
                lcd.print_char(5 + i, 3, match *l {
                    LoopState::Empty     => '\u{2014}', // Em dash
                    LoopState::Recording => '\u{25cf}', // Black circle
                    LoopState::Playing   => '\u{25b6}', // Black right-pointing triangle
                    LoopState::Muted     => '\u{23f8}'  // Double vertical bar
                });
            }
            lcd.update();
        }

        println!("--");
        println!("Pitch range:     [{} - {}]", kb.get_min_note_name(), kb.get_max_note_name());
        println!("Program range:   [{} - {}]", kb.min_program + 1, kb.min_program + kb.program_keys);
        if kb.percussion {
            println!("Percussion")
        }
        else {
            // TODO map current program to instrument name.
            println!("Current program: {}", kb.current_program + 1);
        }
        print!("Loops:           ");
        for l in &self.loop_states {
            let c = match *l {
                LoopState::Empty     => '_',
                LoopState::Recording => 'R',
                LoopState::Playing   => '>',
                LoopState::Muted     => 'M'
            };
            print!("{}", c);
        }
        println!();
    }
}

impl LoopManager for Display {
    fn get_loop_count(&self) -> usize {
        self.loop_states.len()
    }

    fn set_loop_state(&mut self, loop_index : usize, state : LoopState) {
        self.loop_states[loop_index] = state
    }

    fn get_loop_state(&self, loop_index : usize) -> LoopState {
        self.loop_states[loop_index]
    }
}

fn main() {
    // Create a default state and show it.
    let mut display = Display::new(9);
    let mut keyboard = Keyboard::new();
    display.show(&keyboard);

    // Open Jack client and register MIDI input port.
    let (client, _status) = jack::Client::new("midi314-display", jack::ClientOptions::NO_START_SERVER).unwrap();
    let midi_in = client.register_port("midi_in", jack::MidiIn::default()).unwrap();

    // Show MIDI messages.
    let cback = move |_ : &jack::Client, ps : &jack::ProcessScope| -> jack::Control {
        let mut has_event = false;

        for e in midi_in.iter(ps) {
            if keyboard.update(&mut display, e.bytes.to_vec()) {
                has_event = true;
            }
        }

        if has_event {
            display.show(&keyboard);
        }

        jack::Control::Continue
    };

    let active_client = client.activate_async((), jack::ClosureProcessHandler::new(cback)).unwrap();

    // Wait.
    loop {
        thread::sleep(time::Duration::from_secs(1))
    }
}
