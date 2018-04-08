
extern crate jack;
extern crate midi314;

use midi314::{Midi314, LoopManager, LoopState};

#[derive(Clone)]
struct Loop {
    state : LoopState,
    samples_1 : Vec<f32>,
    samples_2 : Vec<f32>
}

impl Loop {
    fn new(l : usize) -> Self {
        Self {
            state     : LoopState::Empty,
            samples_1 : vec![0.0 ; l],
            samples_2 : vec![0.0 ; l]
        }
    }

    fn record(&mut self, length : usize, cursor : usize, in_1 : &[f32], in_2 : &[f32]) {
        let length = if length == 0 {
            self.samples_1.len()
        }
        else {
            length
        };

        let mut in_len = in_1.len();
        let mut in_index = 0;
        let mut cursor = cursor;

        while in_len > 0 {
            let copy_len = if cursor + in_len < length {
                in_len
            }
            else {
                length - cursor
            };

            let src_1  = &in_1[in_index .. in_index + copy_len];
            let src_2  = &in_2[in_index .. in_index + copy_len];
            let dest_1 = &mut self.samples_1[cursor .. cursor + copy_len];
            let dest_2 = &mut self.samples_2[cursor .. cursor + copy_len];
            dest_1.copy_from_slice(src_1);
            dest_2.copy_from_slice(src_2);

            in_len   -= copy_len;
            in_index += copy_len;
            cursor   += copy_len;
            if cursor == length {
                cursor = 0;
            }
        }
    }

    fn play(&self, length : usize, cursor : usize, out_1 : &mut [f32], out_2 : &mut [f32]) {
        let mut cursor = cursor;
        for k in 0 .. out_1.len() {
            out_1[k] += self.samples_1[cursor];
            out_2[k] += self.samples_2[cursor];
            cursor += 1;
            if cursor == length {
                cursor = 0;
            }
        }
    }
}

#[derive(PartialEq, PartialOrd)]
enum LooperState {
    Idle,
    WaitingFirstNote,
    RecordingFirstLoop,
    Running
}

struct Looper {
    state     : LooperState,
    loops     : Vec<Loop>,
    length    : usize,
    cursor    : usize,
    threshold : f32
}

impl Looper {
    fn new(n_loops : usize, max_loop_length : usize, threshold : f32) -> Self {
        Self {
            state     : LooperState::Idle,
            loops     : vec![Loop::new(max_loop_length) ; n_loops],
            cursor    : 0,
            length    : 0,
            threshold : threshold
        }
    }

    pub fn is_recording(&self) -> bool {
        self.loops.iter().any(|ref s| s.state == LoopState::Recording)
    }

    pub fn is_playing(&self) -> bool {
        self.loops.iter().any(|ref s| s.state == LoopState::Playing)
    }

    pub fn has_record(&self) -> bool {
        self.loops.iter().any(|ref s| s.state == LoopState::Playing || s.state == LoopState::Muted)
    }

    pub fn is_empty(&self) -> bool {
        self.loops.iter().all(|ref s| s.state == LoopState::Empty)
    }

    fn update_state(&mut self, in_1 : &[f32], in_2 : &[f32]) {
        match self.state {
            LooperState::Idle =>
                if self.is_recording() {
                    self.state = LooperState::WaitingFirstNote;
                },
            LooperState::WaitingFirstNote =>
                if self.get_level(in_1, in_2) > self.threshold {
                    self.state = LooperState::RecordingFirstLoop;
                    self.length = 0;
                    self.cursor = 0;
                },
            LooperState::RecordingFirstLoop =>
                if self.has_record() {
                    self.state = LooperState::Running;
                    self.length = self.cursor;
                    self.cursor = 0;
                },
            LooperState::Running =>
                if self.is_empty() {
                    self.state = LooperState::Idle;
                }
        }
    }

    fn get_level(&mut self, in_1 : &[f32], in_2 : &[f32]) -> f32 {
        (in_1.iter().fold(0.0, |sum, x| sum + x * x) +
         in_2.iter().fold(0.0, |sum, x| sum + x * x)) /
        (in_1.len() + in_2.len()) as f32
    }

    fn run(&mut self, in_1 : &[f32], in_2 : &[f32], out_1 : &mut [f32], out_2 : &mut [f32]) {
        out_1.copy_from_slice(in_1);
        out_2.copy_from_slice(in_2);

        if self.state >= LooperState::RecordingFirstLoop {
            for l in &mut self.loops {
                match l.state {
                    LoopState::Recording => l.record(self.length, self.cursor, in_1, in_2),
                    LoopState::Playing   => l.play(self.length, self.cursor, out_1, out_2),
                    _                    => ()
                }
            }
            self.cursor += in_1.len();
            if self.state == LooperState::Running {
                if self.cursor >= self.length {
                    self.cursor -= self.length;
                }
            }
        }
    }
}

impl LoopManager for Looper {
    fn set_loop_state(&mut self, loop_index : usize, state : LoopState) {
        let l = &mut self.loops[loop_index];
        l.state = state;
        if state == LoopState::Empty {
            // Clear the loop buffer to delete.
            let zeros = vec![0.0 ; l.samples_1.len()];
            l.samples_1.copy_from_slice(&zeros);
            l.samples_2.copy_from_slice(&zeros);
        }
    }
}

fn main() {
    // Create a default state and show it.
    // TODO Take args from config file.
    let mut m = Midi314::new(Looper::new(9, 48000 * 60, 1.0e-4));

    // Open Jack client and register a MIDI input port and audio I/O ports.
    let (client, _status) = jack::Client::new("midi314-looper", jack::ClientOptions::NO_START_SERVER).unwrap();

    let     midi_in     = client.register_port("midi_in",     jack::MidiIn::default()).unwrap();
    let     audio_in_1  = client.register_port("audio_in_1",  jack::AudioIn::default()).unwrap();
    let     audio_in_2  = client.register_port("audio_in_2",  jack::AudioIn::default()).unwrap();
    let mut audio_out_1 = client.register_port("audio_out_1", jack::AudioOut::default()).unwrap();
    let mut audio_out_2 = client.register_port("audio_out_2", jack::AudioOut::default()).unwrap();

    let cback = move |_ : &jack::Client, ps : &jack::ProcessScope| -> jack::Control {
        // Process MIDI events and update the current state.
        for e in midi_in.iter(ps) {
            m.update(e.bytes.to_vec());
        }

        // Get the current audio buffers.
        let     in_1  = audio_in_1.as_slice(ps);
        let     in_2  = audio_in_2.as_slice(ps);
        let mut out_1 = audio_out_1.as_mut_slice(ps);
        let mut out_2 = audio_out_2.as_mut_slice(ps);

        // Update the looper state.
        m.loop_manager.update_state(&in_1, &in_2);

        // Process the audio data.
        m.loop_manager.run(&in_1, &in_2, &mut out_1, &mut out_2);

        jack::Control::Continue
    };

    let active_client = client.activate_async((), jack::ClosureProcessHandler::new(cback)).unwrap();

    // Wait.
    loop {}
}
