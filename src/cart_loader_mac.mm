#import <AVFoundation/AVFoundation.h>

#include "cart_loader.h"

#include <stdlib.h>
#include <string.h>

/* Max cart length decoded into RAM (10 minutes stereo @ 48 kHz). */
static const long long kCartMaxFrames = 48000LL * 60LL * 10LL;

static int append_pcm_s16le_mono_to_stereo(NSMutableData *accum, const void *bytes,
                                           size_t byte_len, int channels)
{
    const int16_t *in;
    size_t samples;
    size_t i;
    long long out_frames;

    if (!accum || !bytes || byte_len < 2)
        return 0;

    in = (const int16_t*)bytes;
    samples = byte_len / sizeof(int16_t);
    out_frames = (long long)(accum.length / (2 * sizeof(int16_t)));

    if (channels >= 2)
    {
        long long frames = (long long)(samples / 2);

        if (out_frames + frames > kCartMaxFrames)
            return -1;

        [accum appendBytes:bytes length:byte_len];
        return (int)frames;
    }

    if (out_frames + (long long)samples > kCartMaxFrames)
        return -1;

    for (i = 0; i < samples; i++)
    {
        int16_t s = in[i];
        int16_t lr[2] = { s, s };

        [accum appendBytes:lr length:sizeof(lr)];
    }

    return (int)samples;
}

short *cart_load_av_stereo(const char *path, int target_sr, int *out_frames)
{
    short *pcm = NULL;
    int frames = 0;
    int source_sr = 44100;

    if (!path || !path[0] || !out_frames)
        return NULL;

    *out_frames = 0;

    @autoreleasepool {
        NSError *error = nil;
        NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
        AVURLAsset *asset = [AVURLAsset URLAssetWithURL:url options:nil];
        AVAssetReader *reader;
        AVAssetTrack *track;
        AVAssetReaderTrackOutput *output;
        NSDictionary *settings;
        NSMutableData *accum;

        if (!asset)
            return NULL;

        track = [[asset tracksWithMediaType:AVMediaTypeAudio] firstObject];
        if (!track)
            return NULL;

        reader = [[AVAssetReader alloc] initWithAsset:asset error:&error];
        if (!reader)
            return NULL;

        settings = @{
            AVFormatIDKey : @(kAudioFormatLinearPCM),
            AVLinearPCMBitDepthKey : @(16),
            AVLinearPCMIsBigEndianKey : @NO,
            AVLinearPCMIsFloatKey : @NO,
            AVLinearPCMIsNonInterleaved : @NO,
            AVNumberOfChannelsKey : @(2)
        };

        output = [[AVAssetReaderTrackOutput alloc] initWithTrack:track
                                                  outputSettings:settings];
        if (!output)
            return NULL;

        output.alwaysCopiesSampleData = NO;
        [reader addOutput:output];

        if (![reader startReading])
            return NULL;

        accum = [NSMutableData dataWithCapacity:65536];

        while (reader.status == AVAssetReaderStatusReading)
        {
            CMSampleBufferRef sample = [output copyNextSampleBuffer];

            if (!sample)
                break;

            CMBlockBufferRef block = CMSampleBufferGetDataBuffer(sample);
            if (block)
            {
                size_t length = 0;
                char *data_ptr = NULL;
                CMFormatDescriptionRef fmt = CMSampleBufferGetFormatDescription(sample);
                const AudioStreamBasicDescription *asbd =
                    CMAudioFormatDescriptionGetStreamBasicDescription(fmt);
                int channels = asbd ? (int)asbd->mChannelsPerFrame : 2;
                OSStatus st;

                if (asbd && asbd->mSampleRate > 0.0)
                    source_sr = (int)(asbd->mSampleRate + 0.5);

                st = CMBlockBufferGetDataPointer(block, 0, NULL, &length, &data_ptr);
                if (st == kCMBlockBufferNoErr && data_ptr && length > 0)
                {
                    if (append_pcm_s16le_mono_to_stereo(accum, data_ptr, length,
                                                        channels) < 0)
                    {
                        CFRelease(sample);
                        return NULL;
                    }
                }
            }

            CFRelease(sample);
        }

        if (reader.status == AVAssetReaderStatusFailed || accum.length < 4)
            return NULL;

        frames = (int)(accum.length / (2 * sizeof(int16_t)));
        if (frames <= 0)
            return NULL;

        pcm = (short*)malloc((size_t)frames * 2 * sizeof(short));
        if (!pcm)
            return NULL;

        memcpy(pcm, accum.bytes, (size_t)frames * 2 * sizeof(short));
    }

    {
        short *rs = cart_resample_stereo_pcm(pcm, frames, source_sr, target_sr, out_frames);
        free(pcm);
        return rs;
    }
}
