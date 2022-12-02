#include <SDL.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

void emit(float sample);

struct op {
	double freq;
	double phase;
	double level;

	double amplitude;
	double decay;
};

double op_tick(struct op *o, double offset, double srate)
{
	double out = o->level * o->amplitude * sin(M_PI * 2.0 * (o->phase + offset));
	o->amplitude *= 1.0 - (o->decay / srate);
	o->phase = fmod(o->phase + o->freq / srate, 1.0);
	return out;
}

void op_trigger(struct op *o)
{
	o->amplitude = 1.0;
	o->phase = 0.0;
}

struct voice {
	struct op mod;
	struct op car;
};

double voice_tick(struct voice *v, double srate)
{
	double mod = op_tick(&v->mod, 0.0, srate);
	double car = op_tick(&v->car, mod, srate);
	return car;
}

void voice_trigger(struct voice *v)
{
	op_trigger(&v->car);
	op_trigger(&v->mod);
}

struct sequencer {
	double steps;
	bool last_gate;
	struct voice v;
};

double sequencer_tick(struct sequencer *s, double phase, double srate)
{
	bool gate = fmod(phase * s->steps, 1.0) < 0.5;
	if (gate && !s->last_gate) {
		voice_trigger(&s->v);
	}
	s->last_gate = gate;
	return voice_tick(&s->v, srate);
}

void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [OPTION] DIVISION [DIVISION...]\n", name);
	fputs("\tOutputs one or more metronomes at the given divisions of the measure.\n", stderr);
	fputs("Options:\n", stderr);
	fputs("\t-r SAMPLE RATE (default: 48000)\n", stderr);
	fputs("\t-b BPM (default: 100)\n", stderr);
	fputs("\t-f BASE TICK FREQ (default: 120)\n", stderr);
	fputs("\t-a FALLOFF (default: 0.6)\n", stderr);
	fputs("\t-d DECAY (default: 50.0)\n", stderr);
	fputs("\t-v VOLUME (default: 0.5)\n", stderr);
	fputs("\t-m MODULATION LEVEL (default: 0.2)\n", stderr);
}

struct sequencer *seqs = NULL;
int nseqs = 0;
double phase = 0.0;
double bpm = 100.0;

SDL_AudioSpec audiospec = {0};
void audio_cb(void *data, Uint8 *stream, int len)
{
	int i, j;
	double r = audiospec.freq;
	float *buf = (float *)stream;

	for (i = 0; i < len / sizeof(buf[0]); i += 1) {
		double out = 0.0;

		for (j = 0; j < nseqs; j++) {
			out += sequencer_tick(&seqs[j], phase, (double)r);
		}
		buf[i] = fmin(fmax(out, -1.0), 1.0);
		phase = fmod(phase + (bpm / 240.0) / (double)r, 1.0);
	}
}

int main(int argc, char **argv)
{
	int opt, i;
	int err = 1;
	double basefreq = 120.0;
	double volume = 0.5;
	double falloff = 0.6;
	double decay = 50.0;
	double modulation = 0.2;
	SDL_Event e;
	SDL_AudioDeviceID dev = 0;

	SDL_AudioSpec audio_want = {
		.freq = 48000,
		.format = AUDIO_F32,
		.channels = 1,
		.callback = audio_cb,
		.samples = 1024,
	};

	while ((opt = getopt(argc, argv, "r:b:f:a:d:v:m:h")) != -1) {
		switch (opt) {
		case 'r':
			audio_want.freq = atoi(optarg);
			break;
		case 'b':
			bpm = atof(optarg);
			break;
		case 'f':
			basefreq = atof(optarg);
			break;
		case 'a':
			falloff = atof(optarg);
			break;
		case 'd':
			decay = atof(optarg);
			break;
		case 'v':
			volume = atof(optarg);
			break;
		case 'm':
			modulation = atof(optarg);
			break;
		case 'h':
			usage(argv[0]);
			return 0;
			break;
		default:
			usage(argv[0]);
			return 1;
			break;
		}
	}

	nseqs = argc - optind;
	if (!nseqs) {
		usage(argv[0]);
		goto done;
	}

	seqs = calloc(nseqs, sizeof (struct sequencer));
	if (!seqs) {
		fputs("failed to allocate sequences\n", stderr);
		goto done;
	}
	for (i = 0; i < nseqs; i++) {
		int steps = atoi(argv[optind + i]);
		if (!steps) continue;

		seqs[i].steps = steps;
		seqs[i].v = (struct voice){
			.mod = { .freq = (i + 1)*basefreq*1.583, .level = modulation, .decay = decay * 2.0 },
			.car = { .freq = (i + 1)*basefreq, .level = volume, .decay = decay },
		};
		volume *= falloff;
	}

	if (SDL_Init(SDL_INIT_AUDIO|SDL_INIT_EVENTS)) {
		fprintf(stderr, "failed to initialize SDL: %s\n", SDL_GetError());
		goto done;
	}

	dev = SDL_OpenAudioDevice(NULL, 0, &audio_want, &audiospec,
			SDL_AUDIO_ALLOW_FREQUENCY_CHANGE
			|SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
	if (!dev) {
		fprintf(stderr, "failed to open audio device: %s\n", SDL_GetError());
		goto done;
	}

	SDL_PauseAudioDevice(dev, 0);

	while (SDL_WaitEvent(&e)) {
		switch(e.type) {
		case SDL_QUIT:
			err = 0;
			goto done;
		}
	}
done:
	if (dev) SDL_CloseAudioDevice(dev);
	SDL_Quit();
	if (seqs) free(seqs);
	return err;
}
