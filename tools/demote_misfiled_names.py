"""Moves names that are not names from dict.db's names table to its phrases table.

usage: python3 -I tools/demote_misfiled_names.py <dict.db>

A name wins wherever it starts and cuts short any phrase it starts inside (greedy segmentation), so
a misfiled one breaks the words around it: the name 华 split 才华, 繁华 and 年华 apart. Moved are:
- names whose translation starts with a lowercase letter (华 = hoa, 举行 = cử hành, 当天霸 = khi
  Thiên Bá): by their own translation, phrases;
- single-character names (吴, 魏, 尺), which as names cut every phrase they appear in.

Translations do not change: the name's translation becomes the phrase's default meaning, followed
by the phrase's own meanings when it had some. Running it again moves nothing more.
"""
import sqlite3
import sys

if len(sys.argv) != 2:
    sys.exit(__doc__)

db = sqlite3.connect(sys.argv[1])
misfiled = [(key, value) for key, value in db.execute('SELECT original, translated FROM names')
            if value[:1].islower() or len(key) == 1]
with db:
    for key, value in misfiled:
        phrase = db.execute('SELECT translated FROM phrases WHERE original = ?', (key,)).fetchone()
        if phrase is None:
            db.execute('INSERT INTO phrases VALUES (?, ?)', (key, value))
        else:
            meanings = [value] + [m for m in phrase[0].split('\x1f') if m != value]
            db.execute('UPDATE phrases SET translated = ? WHERE original = ?', ('\x1f'.join(meanings), key))
        db.execute('DELETE FROM names WHERE original = ?', (key,))
db.execute('VACUUM')
print(f'moved {len(misfiled)} names to phrases')
