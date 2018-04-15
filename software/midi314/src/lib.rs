
extern crate rimd;
#[macro_use] extern crate num_derive;
extern crate num_traits;
use num_traits::FromPrimitive;

#[derive(PartialEq, Clone, Copy)]
pub enum LoopState {
    Empty,
    Recording,
    Playing,
    Muted
}

pub trait LoopManager {
    fn get_loop_count(&self) -> usize;
    fn set_loop_state(&mut self, loop_index : usize, state : LoopState);
    fn get_loop_state(&self, loop_index : usize) -> LoopState;
}

// Control Change events from 20 to 31 are undefined in the MIDI standard.
#[derive(PartialEq, Clone, Copy, FromPrimitive)]
pub enum CustomCC {
    Record        = 20,
    Play          = 21,
    Mute          = 22,
    Delete        = 23,
    Solo          = 24,
    All           = 25,
    SetMinPitch   = 26,
    SetMinProgram = 27
}

pub struct Midi314<T : LoopManager> {
    pub loop_manager : T,
    pub min_pitch : u32,
    pub min_program : u32,
    pub current_program : u32,
    pub keyboard_width_semi : u32,
    pub program_keys : u32
}

impl<T : LoopManager> Midi314<T> {
    pub fn new(loop_manager : T) -> Self {
        // TODO take default values from config file.
        Self {
            loop_manager        : loop_manager,
            min_pitch           : 48, // C3 TODO map from note name
            min_program         : 0,
            current_program     : 0,
            keyboard_width_semi : 28,
            program_keys        : 10
        }
    }

    pub fn update(&mut self, b : Vec<u8>) -> bool {
        // Convert the raw MIDI data to a MidiMessage.
        let m = rimd::MidiMessage::from_bytes(b);

        match m.status() {
            rimd::Status::ProgramChange => self.program_change(m.data(1)),
            rimd::Status::ControlChange => self.control_change(m.data(1), m.data(2)),
            _                           => false
        }
    }

    pub fn get_min_note_name(&self) -> String {
        rimd::note_num_to_name(self.min_pitch)
    }

    pub fn get_max_note_name(&self) -> String {
        rimd::note_num_to_name(self.min_pitch + self.keyboard_width_semi - 1)
    }

    fn program_change(&mut self, program : u8) -> bool {
        self.current_program = program as u32;
        true
    }

    fn is_solo(&self, loop_index : usize) -> bool {
        // If the loop at the given index is not playing, then it is not in "solo" mode.
        if self.loop_manager.get_loop_state(loop_index) != LoopState::Playing {
            return false
        }
        // If anlther loop is playing, then the loop at the given index is not in "solo" mode.
        for i in 0..self.loop_manager.get_loop_count() {
            if i != loop_index && self.loop_manager.get_loop_state(i) == LoopState::Playing {
                return false
            }
        }
        true
    }

    fn play_solo(&mut self, loop_index : usize) {
        // The loop at the given index must be in playing or muted state.
        let current_state = self.loop_manager.get_loop_state(loop_index);
        if current_state != LoopState::Playing && current_state != LoopState::Muted {
            return
        }
        // Play the loop at the given index.
        self.loop_manager.set_loop_state(loop_index, LoopState::Playing);
        // Mute all other playing loops.
        for i in 0..self.loop_manager.get_loop_count() {
            if i != loop_index && self.loop_manager.get_loop_state(i) == LoopState::Playing {
                self.loop_manager.set_loop_state(i, LoopState::Muted)
            }
        }
    }

    fn play_all(&mut self) {
        // Play all muted loops.
        for i in 0..self.loop_manager.get_loop_count() {
            if self.loop_manager.get_loop_state(i) == LoopState::Muted {
                self.loop_manager.set_loop_state(i, LoopState::Playing)
            }
        }
    }

    fn control_change(&mut self, cc : u8, n : u8) -> bool{
        let index = n as usize;
        let mut result = true;
        match CustomCC::from_u8(cc) {
            Some(CustomCC::Record)        => self.loop_manager.set_loop_state(index, LoopState::Recording),
            Some(CustomCC::Play)          => self.loop_manager.set_loop_state(index, LoopState::Playing),
            Some(CustomCC::Mute)          => self.loop_manager.set_loop_state(index, LoopState::Muted),
            Some(CustomCC::Delete)        => self.loop_manager.set_loop_state(index, LoopState::Empty),
            Some(CustomCC::Solo)          => self.play_solo(index),
            Some(CustomCC::All)           => self.play_all(),
            Some(CustomCC::SetMinPitch)   => self.min_pitch   = n as u32,
            Some(CustomCC::SetMinProgram) => self.min_program = n as u32,
            _                             => result = false
        }
        result
    }
}
