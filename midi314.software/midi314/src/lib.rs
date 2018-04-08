
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
    fn set_loop_state(&mut self, loop_index : usize, state : LoopState);
}

#[derive(PartialEq, Clone, Copy, FromPrimitive)]
pub enum CustomCC {
    Record        = 20,
    Play          = 21,
    Mute          = 22,
    Delete        = 23,
    Solo          = 24,
    SetMinPitch   = 25,
    SetMinProgram = 26
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

    fn program_change(&mut self, p : u8) -> bool {
        self.current_program = p as u32;
        true
    }

    fn control_change(&mut self, cc : u8, n : u8) -> bool{
        let index = n as usize;
        let mut result = true;
        match CustomCC::from_u8(cc) {
            Some(CustomCC::Record)        => self.loop_manager.set_loop_state(index, LoopState::Recording),
            Some(CustomCC::Play)          => self.loop_manager.set_loop_state(index, LoopState::Playing),
            Some(CustomCC::Mute)          => self.loop_manager.set_loop_state(index, LoopState::Muted),
            Some(CustomCC::Delete)        => self.loop_manager.set_loop_state(index, LoopState::Empty),
            Some(CustomCC::Solo)          => (), // TODO
            Some(CustomCC::SetMinPitch)   => self.min_pitch   = n as u32,
            Some(CustomCC::SetMinProgram) => self.min_program = n as u32,
            _                             => result = false
        }
        result
    }
}
