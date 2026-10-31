"""Exercise actual SDL/ADX song endings using a two-second test clip.

The game still decodes the disc music and produces PCM; only its endpoint is
shortened. No save, setting, or soundtrack in the player's install is changed.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def run(game, choice, executable):
    out = ROOT / f"port/build/native-music-tests/{game}-{choice}"
    out.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    env.update(BT3_GS="none", BT3_SOUND="1", SDL_AUDIO_DRIVER="dummy",
               BT3_PACED="1", BT3_DEMO="1", BT3_TAG_TEAM="1",
               BT3_TAG_ROSTER="2,2", BT3_TEST_FRAMES="900",
               BT3_SETTINGS=str(out / "settings.txt"), BT3_SAVES=str(out / "saves"),
               BT3_TEST_MUSIC_SECONDS="2", BT3_TEST_BGM=str(choice),
               BT3_SND_VERBOSE="1", BT3_UPDATE_CHECK="0",
               BT3_SND_DUMP=str(out / "music.raw"),
               BT3_DATA=str(ROOT / ("gamedata-bt4" if game == "bt4" else "gamedata")))
    log = out / "runtime.log"
    with log.open("w") as stream:
        result = subprocess.run([str(executable)], cwd=ROOT,
                                env=env, stdout=stream, stderr=stream, timeout=100)
    text = log.read_text(errors="replace")
    assert result.returncode == 0 and "crashed:" not in text, log
    pcm = (out / "music.raw").read_bytes()
    # More than four two-second stereo clips even at the highest disc rate;
    # sound is still produced near the end, beyond the first playback.
    assert len(pcm) > 4 * 2 * 48000 * 4, (log, len(pcm))
    assert any(pcm[-192000:]), "Music became silent after its first ending"
    songs = re.findall(r"battle music: next (.+)", text)
    if choice == 24:
        assert "battle music: random," in text and len(songs) >= 3, log
        assert all(a != b for a, b in zip(songs, songs[1:])), songs
    else:
        assert "battle music: repeat," in text and not songs, log
        assert text.count("adx: player 0: start ") == 1, "Chosen song was replaced"
    report = dict(game=game, choice=choice, rotations=len(songs), pcm_bytes=len(pcm))
    (out / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print("PASS", report, flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", choices=("bt3", "bt4"))
    parser.add_argument('--exe', type=Path, default=ROOT / ('port/build/bt3.exe' if sys.platform == 'win32' else 'port/build/bt3_64'))
    args = parser.parse_args()
    jobs = [(game, choice) for game in ([args.game] if args.game else ("bt3", "bt4"))
            for choice in (0, 24)]
    with ThreadPoolExecutor(2) as pool:
        list(pool.map(lambda job: run(*job, args.exe), jobs))
