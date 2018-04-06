
extern crate rimd;
#[macro_use] extern crate num_derive;
extern crate num_traits;
use num_traits::FromPrimitive;

#[derive(Clone, Copy)]
pub enum LoopState {
    Empty,
    Recording,
    Playing,
    Muted
}

#[derive(Debug, PartialEq, Clone, Copy, FromPrimitive)]
pub enum CustomCC {
    Record        = 20,
    Play          = 21,
    Mute          = 22,
    Delete        = 23,
    Solo          = 24,
    SetMinPitch   = 25,
    SetMinProgram = 26
}

pub struct State {
    pub loop_states : Vec<LoopState>,
    pub min_pitch : u32,
    pub min_program : u32,
    pub current_program : u32,
    pub keyboard_width_semi : u32,
    pub program_keys : u32
}

impl State {
    pub fn new() -> Self {
        // TODO take default values from config file.
        Self {
            loop_states         : vec![LoopState::Empty ; 9],
            min_pitch           : 48, // C3 TODO map from note name
            min_program         : 0,
            current_program     : 0,
            keyboard_width_semi : 28,
            program_keys        : 10
        }
    }

    pub fn update(&mut self, m : rimd::MidiMessage) -> bool {
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
