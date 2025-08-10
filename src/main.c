#include "stdio.h"
#include "DFT.h"

#include <raylib.h>
#include <stdlib.h>
#include <math.h>
#include <dr_wav.h>
#include <portaudio.h>
#include <pa_ringbuffer.h>
#include <string.h>

#define wave_size 512*2
#define FRAMES_PER_BUFFER 512
//define RING_BUFFER_SIZE 8192
#define RING_BUFFER_SIZE 512*2

#define PLAY_RECONSTRUCTION 0

#ifdef _WIN32
#   include <windows.h>
#   define SLEEP(msecs) Sleep(msecs)
#elif __unix
#   include <time.h>
#   define SLEEP(msecs) do { struct timespec ts; ts.tv_sec = msecs/1000; ts.tv_nsec = (msecs%1000)*1000000; nanosleep(&ts, NULL); } while (0)
#else
#   error "Unknown system"
#endif

typedef struct {
    float* data;
    drwav_uint64 totalFrames;
    drwav_uint64 currentFrame;
    unsigned int channels;

    PaUtilRingBuffer ring;
    float* ringBufferData;
} AudioData;

static int paCallback(const void* input,
                      void* output,
                      unsigned long framesPerBuffer,
                      const PaStreamCallbackTimeInfo* timeInfo,
                      PaStreamCallbackFlags statusFlags,
                      void* userData)
{
    AudioData* audio = (AudioData*)userData;
    float* out = (float*)output;

    drwav_uint64 framesLeft = audio->totalFrames - audio->currentFrame;
    drwav_uint64 framesToWrite = (framesLeft < framesPerBuffer) ? framesLeft : framesPerBuffer;

    for (unsigned long i = 0; i < framesToWrite; i++) {
        float mono = 0.0f;
        for (unsigned int ch = 0; ch < audio->channels; ch++) {
            mono += audio->data[(audio->currentFrame + i) * audio->channels + ch];
        }
        mono /= audio->channels;
        PaUtil_WriteRingBuffer(&audio->ring, &mono, 1);
        out[i * audio->channels] = mono; // left
        if (audio->channels > 1) out[i * audio->channels + 1] = mono; // right
    }

    if (framesToWrite < framesPerBuffer) {
        memset(&out[framesToWrite * audio->channels], 0, sizeof(float) * (framesPerBuffer - framesToWrite) * audio->channels);
    }

    audio->currentFrame += framesToWrite;
    return (audio->currentFrame >= audio->totalFrames) ? paComplete : paContinue;
}

void draw_wave(float* points, unsigned int size, Color col) {
    for (unsigned int p1 = 0; p1 < size-2; p1++) {
        float p1_data = points[p1];
        float p2_data = points[p1+1];
        DrawLine(p1, 200-p1_data*100, p1+1, 200-p2_data*100, col);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <file.wav>\n", argv[0]);
        return 1;
    }

    drwav wav;
    if (!drwav_init_file(&wav, argv[1], NULL)) {
        fprintf(stderr, "Failed to open WAV file.\n");
        return 1;
    }

    float* pcm = malloc(wav.totalPCMFrameCount * wav.channels * sizeof(float));
    if (!pcm) {
        fprintf(stderr, "Memory allocation failed.\n");
        drwav_uninit(&wav);
        return 1;
    }

    drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, pcm);

    #if PLAY_RECONSTRUCTION
    float _imag[wave_size] = {0};
    float _real[wave_size] = {0};
    complex_array _ft = {
        .imaginary = _imag,
        .real = _real,
        .size = wave_size
    };
    float sample[wave_size];
    float _recon[wave_size];
    for (unsigned int i = 0; i < wav.totalPCMFrameCount * wav.channels; i += wave_size) {
        for (unsigned w_i = 0; w_i < wave_size; w_i++) {
            sample[w_i] = pcm[w_i + i];
        }
        dft(_ft, sample, wave_size);
        reconstruct(_recon,_ft,wave_size);
        if (((int)i/wave_size) % 10 == 0) {
            printf("frame [%i/%i]\n",i/wave_size,wav.totalPCMFrameCount * wav.channels/wave_size);
        }
        for (unsigned w_i = 0; w_i < wave_size; w_i++) {
            pcm[w_i + i] = sample[w_i];
        }
    }
    #endif
    
    AudioData audio;
    audio.data = pcm;
    audio.totalFrames = wav.totalPCMFrameCount;
    audio.currentFrame = 0;
    audio.channels = wav.channels;
    audio.ringBufferData = malloc(sizeof(float) * RING_BUFFER_SIZE);
    PaUtil_InitializeRingBuffer(&audio.ring, sizeof(float), RING_BUFFER_SIZE, audio.ringBufferData);

    Pa_Initialize();

    int numDevices = Pa_GetDeviceCount();
    const PaDeviceInfo* info;

    int selectedDevice = 0;

    for (int i = 0; i < numDevices; ++i) {
        info = Pa_GetDeviceInfo(i);
        printf("[%d] %s (%s)\n", i, info->name, Pa_GetHostApiInfo(info->hostApi)->name);
        if (strcmp(info->name, "Default Sink")==0) {selectedDevice = i;}
    }
    
    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(selectedDevice);
    const PaHostApiInfo* host = Pa_GetHostApiInfo(deviceInfo->hostApi);
    if (deviceInfo->maxOutputChannels >= wav.channels) {
        printf("Using device [%d]: %s (%s)\n", selectedDevice, deviceInfo->name, host->name);
    } else {
        fprintf(stderr, "No suitable output device found. index: %i\n",selectedDevice);
        Pa_Terminate();
        drwav_uninit(&wav);
        free(pcm);
        return 1;
    }

    PaStreamParameters outputParams = {
        .device = selectedDevice,
        .channelCount = wav.channels,
        .sampleFormat = paFloat32,
        .suggestedLatency = deviceInfo->defaultLowOutputLatency,
        .hostApiSpecificStreamInfo = NULL
    };

    PaStream* stream;
    Pa_OpenStream(&stream, NULL, &outputParams, wav.sampleRate,
                  FRAMES_PER_BUFFER, paClipOff, paCallback, &audio);

    PaError err = Pa_StartStream(stream);
    if (err != paNoError) {
        fprintf(stderr, "Pa_StartStream failed: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    float wave[wave_size] = {0};
    float ft_wave[wave_size] = {0};
    float recon[wave_size] = {0};

    float imag[wave_size] = {0};
    float real[wave_size] = {0};
    complex_array ft = {
        .imaginary = imag,
        .real = real,
        .size = wave_size
    };

    InitWindow(wave_size, 400, "fourier transform");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        if (Pa_IsStreamActive(stream)) {
            int available = PaUtil_GetRingBufferReadAvailable(&audio.ring);
            if (available >= wave_size) {
                PaUtil_ReadRingBuffer(&audio.ring, wave, wave_size);
                dft(ft, wave, wave_size);
                for (unsigned int i = 0; i < wave_size; i++) {
                    ft_wave[i] = log10f(1 + sqrtf(ft.imaginary[i]*ft.imaginary[i]+ft.real[i]*ft.real[i])) - 1;
                }
                
                //reconstruct(recon, ft, wave_size);
            }
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);
        draw_wave(wave, wave_size, WHITE);
        draw_wave(ft_wave, wave_size, RED);
        //draw_wave(recon, wave_size, GREEN);
        EndDrawing();

        
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    drwav_uninit(&wav);
    free(pcm);
    free(audio.ringBufferData);
    //free(ft.real);
    //free(ft.imaginary);
    //free(recon);

    CloseWindow();
    return 0;
}
