
extern crate jack;
extern crate rimd;
extern crate num_traits;
#[macro_use] extern crate num_derive;
use std::io;
use num_traits::FromPrimitive;

#[derive(Clone, Copy)]
enum LoopState {
    Empty,
    Recording,
    Playing,
    Muted
}

#[derive(Debug, PartialEq, Clone, Copy, FromPrimitive)]
enum CustomCC {
    Record        = 20,
    Play          = 21,
    Mute          = 22,
    Delete        = 23,
    Solo          = 24,
    SetMinPitch   = 25,
    SetMinProgram = 26
}

struct State {
    loop_states : Vec<LoopState>,
    min_pitch : u32,
    min_program : u32,
    current_program : u32,
    keyboard_width_semi : u32,
    program_keys : u32
}

impl State {
    fn update(&mut self, m : rimd::MidiMessage) -> bool {
        match m.status() {
            rimd::Status::ProgramChange => self.program_change(m.data(1)),
            rimd::Status::ControlChange => self.control_change(m.data(1), m.data(2)),
            _                           => false
        }
    }

    fn program_change(&mut self, p : u8) -> bool {
        self.current_program = p as u32;
        true
    }

    fn control_change(&mut self, cc : u8, n : u8) -> bool{
        let index = n as usize;
        let mut result = true;
        match CustomCC::from_u8(cc) {
            Some(CustomCC::Record)        => self.loop_states[index] = LoopState::Recording,
            Some(CustomCC::Play)          => self.loop_states[index] = LoopState::Playing,
            Some(CustomCC::Mute)          => self.loop_states[index] = LoopState::Muted,
            Some(CustomCC::Delete)        => self.loop_states[index] = LoopState::Empty,
            Some(CustomCC::Solo)          => (), // TODO
            Some(CustomCC::SetMinPitch)   => self.min_pitch   = n as u32,
            Some(CustomCC::SetMinProgram) => self.min_program = n as u32,
            _                             => result = false
        }
        result
    }
}

fn update_ui(state : &State) {
    println!("--");
    let min_note = rimd::note_num_to_name(state.min_pitch);
    let max_note = rimd::note_num_to_name(state.min_pitch + state.keyboard_width_semi - 1);

    println!("Pitch range:     [{} - {}]", min_note, max_note);
    println!("Program range:   [{} - {}]", state.min_program + 1, state.min_program + state.program_keys);
    // TODO map current program to instrument name.
    println!("Current program: {}", state.current_program + 1);
    print!("Loops:           ");
    for l in &state.loop_states {
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
    // TODO take default values from config file.
    let mut state = State {
        loop_states         : vec![LoopState::Empty ; 9],
        min_pitch           : 48, // C3 TODO map from note name
        min_program         : 0,
        current_program     : 0,
        keyboard_width_semi : 28,
        program_keys        : 10
    };

    // Open Jack client and register MIDI input port.
    let (client, _status) = jack::Client::new("midi314-display", jack::ClientOptions::NO_START_SERVER).unwrap();
    let midi_in = client.register_port("in", jack::MidiIn::default()).unwrap();

    // Show MIDI messages.
    let cback = move |_ : &jack::Client, ps : &jack::ProcessScope| -> jack::Control {
        let mut has_event = false;

        for e in midi_in.iter(ps) {
            // Convert the raw MIDI data to a MidiMessage.
            let m = rimd::MidiMessage::from_bytes(e.bytes.to_vec());
            if state.update(m) {
                has_event = true;
            }
        }

        if has_event {
            update_ui(&state);
        }

        jack::Control::Continue
    };

    let active_client = client.activate_async((), jack::ClosureProcessHandler::new(cback)).unwrap();

    // Wait.
    println!("Press any key to quit");
    let mut user_input = String::new();
    io::stdin().read_line(&mut user_input).ok();

    // Optional deactivation.
    active_client.deactivate().unwrap();
}
