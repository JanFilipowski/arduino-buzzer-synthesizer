#!/usr/bin/env python3
"""
midi_csv_generator/main.py

Convert a MIDI file into a CSV of note events for the Arduino player.
Reads the tempo from the MIDI file if present; otherwise falls back to the default BPM.
"""

import sys
import os
import mido
import csv
import math
import heapq

# -----------------------------------------------------------------------------
# Settings
# -----------------------------------------------------------------------------
default_bpm = 120
# Default microseconds per beat at default_bpm
default_tempo = 60000000 / default_bpm
num_buzzers = 5      # Number of available buzzers
MARGIN = 0.005       # Safety margin in seconds (5 ms)

# Weights for hybrid preemption ranking
w_dur  = 0.4
w_vel  = 0.3
w_role = 0.2
w_pit  = 0.1

# Map MIDI channel → “role” weight (e.g. melody vs. bass)
role_map = {
    0: 1.0,    # Channel 0 treated as melody
    1: 0.7,    # Channel 1 treated as bass
    # Other channels default to 0.5
}

def get_note_name(note):
    """Convert a MIDI note number to scientific pitch name (e.g. 60 → 'C4')."""
    note_names = ['C', 'C#', 'D', 'D#', 'E', 'F',
                  'F#', 'G', 'G#', 'A', 'A#', 'B']
    octave = (note // 12) - 1
    return f"{note_names[note % 12]}{octave}"

def get_frequency(note):
    """Convert a MIDI note number to frequency in Hz (A4 = 440 Hz)."""
    return 440.0 * (2 ** ((note - 69) / 12))


def midi_to_csv(midi_file_path, output_csv_path):
    """
    Parse the MIDI file and write out a CSV of note events:
      note name, frequency (Hz), start_time (µs), end_time (µs), buzzer index.
    """
    # Load the MIDI file
    try:
        mid = mido.MidiFile(midi_file_path)
    except Exception as e:
        raise SystemExit(f"Error opening MIDI file '{midi_file_path}': {e}")

    ticks_per_beat = mid.ticks_per_beat

    # 1) Extract tempo (µs per beat) from the MIDI file, if present
    tempo = default_tempo
    try:
        for track in mid.tracks:
            for msg in track:
                if msg.type == 'set_tempo':
                    tempo = msg.tempo
                    raise StopIteration
    except StopIteration:
        pass
    except Exception as e:
        print(f"Warning: could not read tempo from MIDI file, using default {default_bpm} BPM: {e}")

    # 2) Parse note_on/note_off events, record start/end times, velocities, and roles
    current_time = 0.0
    ongoing = {}     # note_number → list of dicts {start, velocity, role}
    raw_notes = []   # list of dicts {note, start, end, velocity, role}

    for msg in mid:
        dt = mido.tick2second(msg.time, ticks_per_beat, tempo)
        current_time += dt

        if msg.type == 'note_on' and msg.velocity > 0:
            ongoing.setdefault(msg.note, []).append({
                'start':    current_time,
                'velocity': msg.velocity,
                'role':     role_map.get(msg.channel, 0.5),
            })

        elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
            if msg.note in ongoing and ongoing[msg.note]:
                info = ongoing[msg.note].pop(0)
                raw_notes.append({
                    'note':     msg.note,
                    'start':    info['start'],
                    'end':      current_time,
                    'velocity': info['velocity'],
                    'role':     info['role']
                })
            else:
                print(f"Warning: note_off for {msg.note} at {current_time:.3f}s without matching note_on")

    # 3) Sort by start time
    raw_notes.sort(key=lambda x: x['start'])

    # 4) Assign buzzers with hybrid preemption ranking
    free_buzzers = list(range(1, num_buzzers + 1))
    heapq.heapify(free_buzzers)
    active_heap = []  # elements: (end_time, note_id, buzzer)
    results = []      # final events: {note, start, end, buzzer, velocity, role}

    for note_id, ev in enumerate(raw_notes):
        start = ev['start']
        end   = ev['end']

        # Release buzzers whose notes have ended
        while active_heap and active_heap[0][0] <= start:
            _, old_id, old_bz = heapq.heappop(active_heap)
            heapq.heappush(free_buzzers, old_bz)

        if free_buzzers:
            buzzer = heapq.heappop(free_buzzers)
            assigned_end = end
        else:
            # Compute KeepScore for active notes to choose one to cut
            entries = list(active_heap)
            rem_durs = [(et - results[nid]['start']) for et, nid, _ in entries]
            max_rem = max(rem_durs) if rem_durs else 0

            scores = {}
            mid_pitch = 66
            pitch_range = max(mid_pitch, 127 - mid_pitch)
            for et, nid, bz in entries:
                rem = et - results[nid]['start']
                f_dur  = rem / max_rem if max_rem > 0 else 0
                f_vel  = results[nid]['velocity'] / 127
                f_role = results[nid]['role']
                f_pit  = 1 - abs(results[nid]['note'] - mid_pitch) / pitch_range
                keep_score = w_dur*(1 - f_dur) + w_vel*f_vel + w_role*f_role + w_pit*f_pit
                scores[nid] = (keep_score, bz, et)

            cut_id, (cut_score, cut_bz, cut_end) = min(scores.items(), key=lambda x: x[1][0])
            for i, (et, nid, bz) in enumerate(active_heap):
                if nid == cut_id:
                    active_heap.pop(i)
                    break
            heapq.heapify(active_heap)

            new_end = max(start - MARGIN, results[cut_id]['start'])
            results[cut_id]['end'] = new_end

            buzzer = cut_bz
            assigned_end = end

        results.append({
            'note':     ev['note'],
            'start':    start,
            'end':      assigned_end,
            'buzzer':   buzzer,
            'velocity': ev['velocity'],
            'role':     ev['role']
        })
        heapq.heappush(active_heap, (assigned_end, note_id, buzzer))

    # 5) Write out the CSV file
    with open(output_csv_path, 'w', newline='', encoding='utf-8') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(['note', 'frequency', 'start_us', 'end_us', 'buzzer'])
        for r in results:
            note_name = get_note_name(r['note'])
            freq      = round(get_frequency(r['note']))
            start_us  = int(r['start'] * 1_000_000)
            end_us    = int(r['end']   * 1_000_000)
            writer.writerow([note_name, freq, start_us, end_us, r['buzzer']])


if __name__ == '__main__':
    # Command-line interface
    if len(sys.argv) != 2:
        print(f"Usage: python {os.path.basename(__file__)} input_file.mid")
        sys.exit(1)

    midi_file_path = sys.argv[1]
    if not os.path.isfile(midi_file_path):
        print(f"Error: MIDI file '{midi_file_path}' not found.")
        sys.exit(1)

    base, ext = os.path.splitext(midi_file_path)
    if ext.lower() != '.mid':
        print("Warning: input file does not have .mid extension; proceeding anyway.")

    output_csv_path = base + '.csv'
    midi_to_csv(midi_file_path, output_csv_path)
    print(f"Conversion complete; CSV saved to: {output_csv_path}")
