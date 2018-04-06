
extern crate jack;
extern crate rimd;
extern crate midi314;
use std::io;
use midi314::{State, LoopState};

fn show(state : &State) {
    let min_note = rimd::note_num_to_name(state.min_pitch);
    let max_note = rimd::note_num_to_name(state.min_pitch + state.keyboard_width_semi - 1);

    println!("--");
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
    println!("Press any key to quit");

    // Create a default state and show it.
    let mut state = State::new();
    show(&state);

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
            show(&state);
        }

        jack::Control::Continue
    };

    let active_client = client.activate_async((), jack::ClosureProcessHandler::new(cback)).unwrap();

    // Wait.
    let mut user_input = String::new();
    io::stdin().read_line(&mut user_input).ok();

    // Optional deactivation.
    active_client.deactivate().unwrap();
}
