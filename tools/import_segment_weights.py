"""Replaces dict.db's segment weights with those from a word-frequency list.

usage: python3 -I tools/import_segment_weights.py <frequency list> <dict.db> [--force]

The list has one word and its count per line, in Simplified Chinese (conversions work on
Simplified text): jieba's dict.txt format ("word count [tag]"), or "word<TAB>count", or
"word,count". Lines starting with # and lines that do not parse are skipped (and counted). A word
listed twice has its counts added up. The default source is jieba's main dictionary
(https://github.com/fxsjy/jieba, jieba/dict.txt, MIT licence, about 60 million words counted).

What it writes, and nothing else (entries, name sets and history are left alone):
- segment_weights: every listed word with weight round(100 * ln(count / total)), whether or not it
  is an entry, so a word added later in the app takes its weight from here (io_insert);
- segment_settings: unseen_weight, what a character no weighted word covers weighs: as much as the
  rarest listed word, round(100 * ln(smallest count / total)). It depends on the list (on how deep
  its counts go), so it comes with it; multiplying every count by the same factor changes nothing.
The core gives every other entry the weight of its likeliest cut into weighted entries each time
it loads (Dictionary::compose_weights), so nothing else needs rebuilding.

Close the app first. The database is copied to <dict.db>.bak before anything changes. The import
stops, unless --force is given, when the list covers far fewer entries than the current weights do:
usually a wrong format or a Traditional Chinese list.
"""
import math
import os
import re
import shutil
import sqlite3
import sys

args = [a for a in sys.argv[1:] if a != '--force']
force = '--force' in sys.argv
if len(args) != 2:
    sys.exit(__doc__)
list_path, db_path = args

# ---- Read the list ---------------------------------------------------------
counts, skipped = {}, 0
with open(list_path, encoding='utf-8-sig') as source:
    for line in source:
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        fields = re.split(r'[\t, ]+', line)
        try:
            word, count = fields[0], float(fields[1])
        except (IndexError, ValueError):
            skipped += 1
            continue
        if count <= 0 or not word:
            skipped += 1
            continue
        counts[word] = counts.get(word, 0) + count
if not counts:
    sys.exit(f'No words read from {list_path} ({skipped} lines skipped).')
total = sum(counts.values())
unseen_p = min(counts.values()) / total

# ---- Check it against the dictionary --------------------------------------
if not os.path.exists(db_path):
    sys.exit(f'{db_path} does not exist.')
if os.path.exists(db_path + '-journal'):
    sys.exit(f'{db_path}-journal exists: the app may have the dictionary open. Close it first.')
db = sqlite3.connect(db_path)
entries = {key for (key,) in db.execute('SELECT original FROM phrases UNION SELECT original FROM names')}
covered = sum(word in entries for word in counts)
has_table = db.execute("SELECT 1 FROM sqlite_master WHERE name = 'segment_weights'").fetchone()
covered_before = db.execute('SELECT count(*) FROM segment_weights WHERE original IN '
                            '(SELECT original FROM phrases UNION SELECT original FROM names)'
                            ).fetchone()[0] if has_table else 0

print(f'{list_path}: {len(counts)} words, {total:.0f} counted in all, {skipped} lines skipped.')
print(f'Entries with a frequency: {covered} of {len(entries)} (now {covered_before}).')
if not force and (covered < covered_before / 2 or covered < 10000):
    sys.exit('Stopped: the list covers too few entries (a wrong format or Traditional Chinese?). '
             'Run again with --force to import it anyway.')

# ---- Replace the weights ----------------------------------------------------
db.close()
shutil.copy2(db_path, db_path + '.bak')
db = sqlite3.connect(db_path)


def weight(p):
    # The core keeps weights in 16 bits.
    return max(-32000, min(0, round(100 * math.log(p))))


with db:
    db.execute('DROP TABLE IF EXISTS segment_weights')
    db.execute('CREATE TABLE segment_weights (original TEXT PRIMARY KEY, weight INTEGER NOT NULL) WITHOUT ROWID')
    db.executemany('INSERT INTO segment_weights VALUES (?, ?)',
                   ((word, weight(count / total)) for word, count in counts.items()))
    db.execute('CREATE TABLE IF NOT EXISTS segment_settings (name TEXT PRIMARY KEY, value INTEGER NOT NULL) WITHOUT ROWID')
    db.execute("INSERT OR REPLACE INTO segment_settings VALUES ('unseen_weight', ?)", (weight(unseen_p),))
db.execute('VACUUM')
db.close()
print(f'Imported; unseen characters weigh {weight(unseen_p)}. The previous database is in {db_path}.bak.')
print('Before relying on a new source, compare it with HanviSegDiff (bench/segdiff.cpp) on a few texts.')
