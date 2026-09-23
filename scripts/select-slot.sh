#!/usr/bin/env bash
# Swap which firmware boots, without reflashing anything.
#   scripts/select-slot.sh slot0   -> ota_0 @0x10000
#   scripts/select-slot.sh slot1   -> ota_1 @0x7f0000
# What is in each slot changes over time (ESPHome OTA alternates slots); read the
# app descriptors with esptool if unsure. Aliases stock/crosspoint/esphome are historical.
# Reads the otadata partition, checks the target slot holds an app image, then
# writes a new boot entry with the next sequence number (same rule as the
# ESP-IDF bootloader and the CrossPoint web flasher: slot = (seq-1) % 2).
# The entry is written as VALID, not NEW: the stock bootloader has rollback
# enabled and the stock app does not confirm itself in time, so a NEW entry
# gets ABORTED on the next reset and the device falls back to the other slot.
set -euo pipefail
cd "$(dirname "$0")/.."
WANT="${1:?usage: $0 slot0|slot1 [port]   (aliases: stock=slot1, crosspoint/esphome=slot0)}"
PORT="${2:-/dev/ttyACM0}"
case "$WANT" in
  slot1|stock)               SLOT=1; APP=0x7f0000 ;;
  slot0|crosspoint|esphome)  SLOT=0; APP=0x10000 ;;
  *) echo "unknown target '$WANT' (slot0|slot1; aliases stock=slot1, crosspoint/esphome=slot0)"; exit 1 ;;
esac
T=$(mktemp -d)
esptool --chip esp32s3 --port "$PORT" read-flash 0xe000 0x2000 "$T/otadata.bin" >/dev/null
esptool --chip esp32s3 --port "$PORT" read-flash "$APP" 16 "$T/app.bin" >/dev/null
python3 - "$T" "$SLOT" <<'PY'
import sys,struct,zlib
d,slot=sys.argv[1],int(sys.argv[2])
if open(f'{d}/app.bin','rb').read(1)!=b'\xe9':
    sys.exit(f'target slot {slot} does not contain an app image; refusing')
ota=open(f'{d}/otadata.bin','rb').read()
def entry(sec):
    e=ota[sec*4096:sec*4096+32]; seq,state,crc=struct.unpack('<I',e[:4])[0],struct.unpack('<I',e[24:28])[0],e[28:32]
    ok=seq!=0xffffffff and crc==struct.pack('<I',zlib.crc32(e[:4],0xffffffff)&0xffffffff) and state not in (3,4)
    return seq if ok else None
seqs=[(s,entry(s)) for s in (0,1) if entry(s) is not None]
cur_sec,cur_seq=max(seqs,key=lambda x:x[1]) if seqs else (-1,0)
cur_slot=(cur_seq-1)%2 if seqs else None
print(f'current: seq {cur_seq} -> slot {cur_slot}')
if cur_slot==slot: print('already selected, nothing to do'); open(f'{d}/skip','w'); sys.exit()
new=cur_seq+1
while (new-1)%2!=slot: new+=1
target=0 if cur_sec<0 else 1-cur_sec
e=struct.pack('<I',new)+b'\xff'*20+struct.pack('<I',2)+struct.pack('<I',zlib.crc32(struct.pack('<I',new),0xffffffff)&0xffffffff)
open(f'{d}/sector.bin','wb').write(e+b'\xff'*(4096-len(e)))
open(f'{d}/offset','w').write(hex(0xe000+target*4096))
print(f'writing seq {new} -> slot {slot} in otadata sector {target}')
PY
if [ -f "$T/skip" ]; then rm -rf "$T"; exit 0; fi
esptool --chip esp32s3 --port "$PORT" write-flash "$(cat "$T/offset")" "$T/sector.bin" | grep -i 'wrote\|verified\|error'
rm -rf "$T"
echo "Device reset; now booting $WANT."
