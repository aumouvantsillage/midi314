
extern crate jack;
extern crate midi314;
extern crate pcd8544;

use std::{thread, time};
use midi314::{Midi314, LoopManager, LoopState};
use pcd8544::{PCD8544};

const LCD_RST : u64 = 24;
const LCD_DC  : u64 = 25;
const LCD_SPI : &'static str = "/dev/spidev0.0";

struct Display {
    loop_states : Vec<LoopState>
}

impl Display {
    fn new(n : usize) -> Self {
        Self {
            loop_states : vec![LoopState::Empty ; n]
        }
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

fn show(m : &Midi314<Display>) {
    println!("--");
    println!("Pitch range:     [{} - {}]", m.get_min_note_name(), m.get_max_note_name());
    println!("Program range:   [{} - {}]", m.min_program + 1, m.min_program + m.program_keys);
    if m.percussion {
        println!("Percussion")
    }
    else {
        // TODO map current program to instrument name.
        println!("Current program: {}", m.current_program + 1);
    }
    print!("Loops:           ");
    for l in &m.loop_manager.loop_states {
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

fn main() {
    let lcd = PCD8544::new(LCD_DC, LCD_RST, LCD_SPI);
    match lcd {
        Ok(mut l) => l.display().unwrap(),
        _         => println!("No LCD display detected")
    }

    // Create a default state and show it.
    let mut m = Midi314::new(Display::new(9));
    show(&m);

    // Open Jack client and register MIDI input port.
    let (client, _status) = jack::Client::new("midi314-display", jack::ClientOptions::NO_START_SERVER).unwrap();
    let midi_in = client.register_port("midi_in", jack::MidiIn::default()).unwrap();

    // Show MIDI messages.
    let cback = move |_ : &jack::Client, ps : &jack::ProcessScope| -> jack::Control {
        let mut has_event = false;

        for e in midi_in.iter(ps) {
            if m.update(e.bytes.to_vec()) {
                has_event = true;
            }
        }

        if has_event {
            show(&m);
        }

        jack::Control::Continue
    };

    let active_client = client.activate_async((), jack::ClosureProcessHandler::new(cback)).unwrap();

    // Wait.
    loop {
        thread::sleep(time::Duration::from_secs(1))
    }
}
