
extern crate jack;
extern crate midi314;

use std::{thread, time};
use midi314::{Keyboard, LoopManager, LoopState};

#[derive(Clone)]
struct Loop {
    state : LoopState,
    state_prev : LoopState,
    transition_time : usize,
    samples_1 : Vec<f32>,
    samples_2 : Vec<f32>
}

impl Loop {
    fn new(l : usize) -> Self {
        Self {
            state           : LoopState::Empty,
            state_prev      : LoopState::Empty,
            transition_time : 0,
            samples_1       : vec![0.0 ; l],
            samples_2       : vec![0.0 ; l]
        }
    }

    fn set_state(&mut self, time : usize, state : LoopState) {
        self.state = state;
        self.transition_time = time;
    }

    fn run(&mut self, from : usize, to : usize, cursor : usize, in_1 : &[f32], in_2 : &[f32], out_1 : &mut [f32], out_2 : &mut [f32]) {
        match self.state {
            LoopState::Empty =>
                if self.state_prev != LoopState::Empty {
                    // Clear the loop buffer to delete.
                    let zeros = vec![0.0 ; self.samples_1.len()];
                    self.samples_1.copy_from_slice(&zeros);
                    self.samples_2.copy_from_slice(&zeros);
                },
            LoopState::Recording =>
                if self.state_prev != LoopState::Recording {
                    let in_1 = &in_1[self.transition_time..];
                    let in_2 = &in_2[self.transition_time..];
                    let cursor = cursor + self.transition_time;
                    self.record(from, to, cursor, in_1, in_2);
                }
                else {
                    self.record(from, to, cursor, in_1, in_2)
                },
            LoopState::Playing => {
                if self.state_prev == LoopState::Recording {
                    // When transitioning from the Recording to the Playing state,
                    // finish recording the beginning of the input buffer (until transition time)
                    // into the loop buffer at the cursor,
                    let in_1 = &in_1[0..self.transition_time];
                    let in_2 = &in_2[0..self.transition_time];
                    self.record(from, to, cursor, in_1, in_2);
                }
                if self.state_prev != LoopState::Playing {
                    // When entering the Playing state, start playing from the transition time
                    // into the end of the output buffer.
                    let out_1 = &mut out_1[self.transition_time..];
                    let out_2 = &mut out_2[self.transition_time..];
                    let cursor = cursor + self.transition_time;
                    self.play(from, to, cursor, out_1, out_2)
                }
                else {
                    self.play(from, to, cursor, out_1, out_2)
                }
            },
            _ => ()
        }

        self.state_prev = self.state;
    }

    fn record(&mut self, from : usize, to : usize, cursor : usize, in_1 : &[f32], in_2 : &[f32]) {
        let length = if to == 0 {
            self.samples_1.len()
        }
        else {
            to
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
                cursor = from;
            }
        }
    }

    fn play(&self, from : usize, to : usize, cursor : usize, out_1 : &mut [f32], out_2 : &mut [f32]) {
        let mut cursor = cursor;
        for k in 0 .. out_1.len() {
            if cursor >= to {
                cursor -= to - from;
            }
            out_1[k] += self.samples_1[cursor];
            out_2[k] += self.samples_2[cursor];
            cursor += 1;
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
    from      : usize,
    to        : usize,
    cursor    : usize,
    threshold : f32
}

impl Looper {
    fn new(n_loops : usize, max_loop_length : usize, threshold : f32) -> Self {
        Self {
            state     : LooperState::Idle,
            loops     : vec![Loop::new(max_loop_length) ; n_loops],
            cursor    : 0,
            from      : 0,
            to        : 0,
            threshold : threshold
        }
    }

    pub fn is_recording(&self) -> bool {
        self.loops.iter().any(|ref s| s.state == LoopState::Recording)
    }

    pub fn recording_end_time(&self) -> Option<usize> {
        for l in &self.loops {
            if l.state_prev == LoopState::Recording && l.state >= LoopState::Playing {
                return Some(l.transition_time)
            }
        }
        None
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
            LooperState::WaitingFirstNote => {
                let from = in_1.iter().zip(in_2.iter()).position(|(&x, &y)| x * x + y * y > self.threshold);
                if let Some(f) = from  {
                    self.state = LooperState::RecordingFirstLoop;
                    self.from = f;
                    self.to = 0;
                    self.cursor = 0;
                }
            },
            LooperState::RecordingFirstLoop =>
                if let Some(time) = self.recording_end_time() {
                    self.state = LooperState::Running;
                    self.to = self.cursor + time;
                },
            LooperState::Running =>
                if self.is_empty() {
                    self.state = LooperState::Idle;
                }
        }
    }

    fn run(&mut self, in_1 : &[f32], in_2 : &[f32], out_1 : &mut [f32], out_2 : &mut [f32]) {
        out_1.copy_from_slice(in_1);
        out_2.copy_from_slice(in_2);

        if self.state >= LooperState::RecordingFirstLoop {
            for l in &mut self.loops {
                l.run(self.from, self.to, self.cursor, in_1, in_2, out_1, out_2);
            }
            self.cursor += in_1.len();
            if self.state == LooperState::Running && self.cursor >= self.to {
                self.cursor = self.from + (self.cursor - self.to);
            }
        }
    }
}

impl LoopManager for Looper {
    fn get_loop_count(&self) -> usize {
        self.loops.len()
    }

    fn set_loop_state(&mut self, loop_index : usize, time : usize, state : LoopState) {
        self.loops[loop_index].set_state(time, state);
    }

    fn get_loop_state(&self, loop_index : usize) -> LoopState {
        self.loops[loop_index].state
    }
}

fn main() {
    // Create a default state and show it.
    // TODO Take args from config file.
    let mut looper = Looper::new(9, 48000 * 60, 1.0e-4);
    let mut keyboard = Keyboard::new();

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
            keyboard.update(&mut looper, e.time as usize, e.bytes.to_vec());
        }

        // Get the current audio buffers.
        let     in_1  = audio_in_1.as_slice(ps);
        let     in_2  = audio_in_2.as_slice(ps);
        let mut out_1 = audio_out_1.as_mut_slice(ps);
        let mut out_2 = audio_out_2.as_mut_slice(ps);

        // Update the looper state.
        looper.update_state(&in_1, &in_2);

        // Process the audio data.
        looper.run(&in_1, &in_2, &mut out_1, &mut out_2);

        jack::Control::Continue
    };

    let _active_client = client.activate_async((), jack::ClosureProcessHandler::new(cback)).unwrap();

    // Wait.
    loop {
        thread::sleep(time::Duration::from_secs(1))
    }
}
