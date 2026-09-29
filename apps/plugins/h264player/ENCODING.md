# Making video files for h264player

h264player plays **H.264 baseline video with MP2 or MP3 audio, in an MPEG program stream (PS)**. Any ffmpeg with `libx264` and the `mp2` encoder can make one. Name the output `something.h264` (the extension the plugin is registered for) and copy it to the device.

## Quick start (320x240 screens, e.g. Eros Q)

```sh
ffmpeg -i input.mp4 \
  -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2,setsar=1" \
  -r 24 \
  -c:v libx264 -profile:v baseline -level 2.1 -pix_fmt yuv420p \
  -x264-params "ref=3:bframes=0:keyint=48:scenecut=0:bitrate=400:vbv-maxrate=600:vbv-bufsize=1200" \
  -c:a mp2 -b:a 128k -ar 44100 -ac 2 \
  -f vob output.h264
```

This command was tested: a 1280x720 H.264/AAC MP4 was converted with it, and the result played through the plugin in the simulator with every frame identical to ffmpeg's own decode, and seeking worked.

What each part does:

| Option | Why |
|---|---|
| `scale ... pad` | Fits the picture inside 320x240 keeping its aspect ratio, with black bars. Change 320:240 for other screens. |
| `-r 24` | Frame rate. Fewer frames means less decoding work. 15-24 is a good range. |
| `-profile:v baseline` | **Required.** The decoder only supports baseline: no B-frames, no CABAC, progressive only. |
| `-level 2.1` | Limits the decoder's frame buffering to what QVGA needs. |
| `-pix_fmt yuv420p` | Required (4:2:0 chroma). |
| `ref=3` | Reference frames. **Keep at 4 or fewer**: each QVGA frame takes about 115 KB of the 2 MiB decoder memory. |
| `bframes=0` | Redundant with baseline, but explicit. |
| `keyint=48` | A keyframe (IDR) every 2 seconds at 24 fps. Seeking and recovery from dropped frames restart at keyframes, so a longer interval means coarser seeking. Use about 2 seconds of frames. |
| `scenecut=0` | Stops x264 inserting extra keyframes so the interval stays regular (optional). |
| `bitrate=400 ...` | Video bitrate in kbps, capped by `vbv-maxrate`/`vbv-bufsize` so bursts stay small. |
| `-c:a mp2 -b:a 128k -ar 44100` | Audio. See below. |
| `-f vob` | Writes an MPEG-2 program stream. |

### Audio

- MP2 (`-c:a mp2`) is the safe choice and the one tested.
- MP3 (`-c:a libmp3lame -b:a 128k`) should also work, as it goes through the same libmad decoder mpegplayer uses. It has not been tested here.
- Use 44100 Hz stereo (or mono with `-ac 1` to save space). AAC and Vorbis are **not** supported in this container by this player.

## Choosing quality

| Goal | Video bitrate | Approx. size per hour (with 128 kbps audio) |
|---|---|---|
| Small | `bitrate=250` | ~170 MB |
| Default | `bitrate=400` | ~235 MB |
| Higher quality | `bitrate=700` | ~370 MB |

For quality-based encoding instead of a target bitrate, replace the `bitrate=..`, `vbv-*` settings with `-crf 24` and add `vbv-maxrate=800:vbv-bufsize=1600` to `-x264-params`.

Busier or faster video needs more bitrate for the same look, and costs more decoding time.

## Other screen sizes

The plugin scales nothing itself: it centres the picture and crops or letterboxes. Make the video no larger than the screen. For a 480x320 screen, use `scale=480:320` and `pad=480:320`, and expect roughly double the decoding cost (a bigger picture also needs bigger frame buffers, so `ref=2` is safer).

## Batch convert a folder

```sh
mkdir -p out
for f in *.mp4 *.mkv; do
  ffmpeg -n -i "$f" \
    -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2,setsar=1" -r 24 \
    -c:v libx264 -profile:v baseline -level 2.1 -pix_fmt yuv420p \
    -x264-params "ref=3:bframes=0:keyint=48:scenecut=0:bitrate=400:vbv-maxrate=600:vbv-bufsize=1200" \
    -c:a mp2 -b:a 128k -ar 44100 -ac 2 -f vob "out/${f%.*}.h264"
done
```

## Checking a file

```sh
ffprobe -v error -show_entries stream=codec_name,profile,width,height,r_frame_rate,id -of compact output.h264
```

You should see `h264` with profile `Constrained Baseline` (or `Baseline`), and an audio stream `mp2`. The video stream id will be `0x1e2`; the player accepts any video id from `0xE0` to `0xEF`.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| "Unsupported format" | Not baseline profile, or not in an MPEG program stream (for example an `.mp4` renamed to `.h264`). |
| Plays for a moment then stops or shows garbage | More than about 4 reference frames, or a level much higher than 2.1 asking for more memory than the decoder has. |
| Stutters or drops frames | Too much work for the device: lower `-r`, lower the bitrate, or lower the resolution. The plugin's frame-skip setting recovers at the next keyframe. |
| Seeking jumps far | `keyint` is too large. |
| No sound | Audio not MP2/MP3. Use 44100 Hz, the rate tested. |
| Colours or shape look wrong | Missing `-pix_fmt yuv420p`, or a non-square pixel aspect (keep `setsar=1`). |

## Not supported

B-frames, CABAC (Main/High profile), interlaced video, 10-bit, AAC audio in this container, and MP4/MKV containers. Use the command above to convert.
