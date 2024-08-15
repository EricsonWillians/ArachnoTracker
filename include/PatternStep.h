#pragma once

#include "Note.h"
#include "Instrument.h"

class PatternStep {
public:
    PatternStep(Note* note, Instrument* instrument) 
        : note(note), instrument(instrument) {}

    Note* getNote() const { return note; }
    Instrument* getInstrument() const { return instrument; }

private:
    Note* note;
    Instrument* instrument;
};
