// gbarun: headless, scripted mGBA runner (links against libmgba 0.10).
// Usage: gbarun [--state FILE] [--syms FILE] [--sav FILE] [--rtc-epoch MS] [--verbose] ROM SCRIPT OUTDIR
// --syms takes `nm` output ("ADDR TYPE NAME" per line); run.sh generates it from the .elf.
// See README section in run.sh for the script syntax.
#include <mgba/flags.h> // must come first: struct layouts depend on these flags
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/log.h>
#include <mgba/core/serialize.h>
#include <mgba-util/vfs.h>
#include <mgba-util/png-io.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

static struct mCore* core;
static color_t* vbuf;
static unsigned W, H;
static const char* outdir;
static unsigned long frames;
static int verbose;

static void quietLog(struct mLogger* l, int cat, enum mLogLevel lvl, const char* fmt, va_list args) {
	(void) l;
	if (!verbose && !(lvl & (mLOG_FATAL))) return;
	fprintf(stderr, "[mgba %s] ", mLogCategoryName(cat));
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
}
static struct mLogger logger = { .log = quietLog };

static void die(int line, const char* fmt, ...) {
	va_list a;
	va_start(a, fmt);
	fprintf(stderr, "error (script line %d): ", line);
	vfprintf(stderr, fmt, a);
	fputc('\n', stderr);
	va_end(a);
	exit(1);
}

static const struct { const char* name; int bit; } KEYS[] = {
	{"A", 0}, {"B", 1}, {"SELECT", 2}, {"START", 3}, {"RIGHT", 4}, {"LEFT", 5},
	{"UP", 6}, {"DOWN", 7}, {"R", 8}, {"L", 9},
};

static int parseKeys(const char* s, uint32_t* out) {
	char buf[256];
	snprintf(buf, sizeof(buf), "%s", s);
	uint32_t k = 0;
	for (char* tok = strtok(buf, "+,|"); tok; tok = strtok(NULL, "+,|")) {
		int found = 0;
		for (size_t i = 0; i < sizeof(KEYS) / sizeof(*KEYS); ++i) {
			if (!strcasecmp(tok, KEYS[i].name)) { k |= 1u << KEYS[i].bit; found = 1; }
		}
		if (!found) return 0;
	}
	*out = k;
	return 1;
}

static void runFrames(long n, uint32_t keys) {
	core->setKeys(core, keys);
	for (long i = 0; i < n; ++i) {
		core->runFrame(core);
		++frames;
	}
}

static char* outPath(const char* name, const char* ext) {
	static char p[4096];
	if (name[0] == '/' || (name[0] == '.' && name[1] == '/'))
		snprintf(p, sizeof(p), "%s%s", name, strchr(strrchr(name, '/'), '.') ? "" : ext);
	else
		snprintf(p, sizeof(p), "%s/%s%s", outdir, name, strchr(name, '.') ? "" : ext);
	return p;
}

static void screenshot(int line, const char* name) {
	const char* path = outPath(name, ".png");
	struct VFile* vf = VFileOpen(path, O_CREAT | O_TRUNC | O_WRONLY);
	if (!vf) die(line, "cannot open %s: %s", path, strerror(errno));
	png_structp png = PNGWriteOpen(vf);
	png_infop info = PNGWriteHeader(png, W, H);
	// color_t is 32-bit XBGR8 (bytes R,G,B,X); PNGWritePixels handles that layout.
	if (!PNGWritePixels(png, W, H, W, vbuf)) die(line, "PNG write failed");
	PNGWriteClose(png, info);
	vf->close(vf);
	printf("frame %lu: shot %s\n", frames, path);
}

static void saveState(int line, const char* name) {
	const char* path = outPath(name, ".ss0");
	struct VFile* vf = VFileOpen(path, O_CREAT | O_TRUNC | O_RDWR);
	if (!vf) die(line, "cannot open %s", path);
	if (!mCoreSaveStateNamed(core, vf, SAVESTATE_SAVEDATA | SAVESTATE_RTC | SAVESTATE_METADATA | SAVESTATE_SCREENSHOT))
		die(line, "savestate failed");
	vf->close(vf);
	printf("frame %lu: savestate %s\n", frames, path);
}

static void loadState(int line, const char* path) {
	struct VFile* vf = VFileOpen(path, O_RDONLY);
	if (!vf) die(line, "cannot open state %s", path);
	if (!mCoreLoadStateNamed(core, vf, SAVESTATE_SAVEDATA | SAVESTATE_RTC | SAVESTATE_SCREENSHOT))
		die(line, "loadstate failed for %s", path);
	vf->close(vf);
	printf("frame %lu: loadstate %s\n", frames, path);
}

static struct { char* name; uint32_t addr; }* syms;
static size_t nsyms;

static void loadSyms(const char* path) {
	FILE* f = fopen(path, "r");
	if (!f) { fprintf(stderr, "cannot open symbol map %s\n", path); exit(1); }
	char l[1024], name[960], type[8];
	unsigned long a;
	size_t cap = 0;
	while (fgets(l, sizeof(l), f)) {
		if (sscanf(l, "%lx %7s %959s", &a, type, name) != 3) continue;
		if (nsyms == cap) { cap = cap ? cap * 2 : 4096; syms = realloc(syms, cap * sizeof(*syms)); }
		syms[nsyms].name = strdup(name);
		syms[nsyms].addr = (uint32_t) a;
		++nsyms;
	}
	fclose(f);
}

static int resolveAddr(int line, const char* tok, uint32_t* addr) {
	char* end;
	unsigned long v = strtoul(tok, &end, 0);
	if (*end == '\0') { *addr = v; return 1; }
	// SYMBOL or SYMBOL+OFFSET
	char base[512];
	snprintf(base, sizeof(base), "%s", tok);
	unsigned long off = 0;
	char* plus = strchr(base, '+');
	if (plus) { *plus = '\0'; off = strtoul(plus + 1, NULL, 0); }
	for (size_t i = 0; i < nsyms; ++i) {
		if (!strcmp(syms[i].name, base)) { *addr = syms[i].addr + off; return 1; }
	}
	die(line, "unknown symbol '%s' (needs --syms / an .elf next to the ROM)", base);
	return 0;
}

int main(int argc, char** argv) {
	const char* state = NULL;
	const char* symsPath = NULL;
	const char* sav = NULL;
	long long rtcEpoch = 1704110400000LL; // 2024-01-01 12:00:00 UTC; deterministic RTC
	int argi = 1;
	for (; argi < argc && argv[argi][0] == '-' && argv[argi][1] == '-'; ++argi) {
		const char* o = argv[argi];
		if (!strcmp(o, "--state") && argi + 1 < argc) state = argv[++argi];
		else if (!strcmp(o, "--syms") && argi + 1 < argc) symsPath = argv[++argi];
		else if (!strcmp(o, "--sav") && argi + 1 < argc) sav = argv[++argi];
		else if (!strcmp(o, "--rtc-epoch") && argi + 1 < argc) rtcEpoch = atoll(argv[++argi]);
		else if (!strcmp(o, "--verbose")) verbose = 1;
		else { fprintf(stderr, "unknown option %s\n", o); return 2; }
	}
	if (argc - argi != 3) {
		fprintf(stderr, "usage: %s [--state FILE] [--syms FILE] [--sav FILE] [--rtc-epoch MS] [--verbose] ROM SCRIPT OUTDIR\n", argv[0]);
		return 2;
	}
	const char* rom = argv[argi];
	const char* script = argv[argi + 1];
	outdir = argv[argi + 2];
	{ // mkdir -p
		char d[4096]; snprintf(d, sizeof(d), "%s", outdir);
		for (char* c = d + 1; *c; ++c) if (*c == '/') { *c = 0; mkdir(d, 0755); *c = '/'; }
		mkdir(d, 0755);
	}

	mLogSetDefaultLogger(&logger);
	core = mCoreFind(rom);
	if (!core) { fprintf(stderr, "unsupported ROM %s\n", rom); return 1; }
	core->init(core);
	mCoreInitConfig(core, NULL);
	core->desiredVideoDimensions(core, &W, &H);
	vbuf = calloc(W * H, BYTES_PER_PIXEL);
	core->setVideoBuffer(core, vbuf, W);
	core->setAudioBufferSize(core, 2048);
	if (!mCoreLoadFile(core, rom)) { fprintf(stderr, "failed to load %s\n", rom); return 1; }
	if (sav) {
		// Loaded as a temporary save: the file on disk is never written.
		struct VFile* vf = VFileOpen(sav, O_RDONLY);
		if (!vf || !core->loadTemporarySave(core, vf)) { fprintf(stderr, "failed to load save %s\n", sav); return 1; }
	}
	core->rtc.override = RTC_FAKE_EPOCH;
	core->rtc.value = rtcEpoch;
	if (symsPath) loadSyms(symsPath);
	core->reset(core);
	if (state) loadState(0, state);

	FILE* f = strcmp(script, "-") ? fopen(script, "r") : stdin;
	if (!f) { fprintf(stderr, "cannot open script %s\n", script); return 1; }
	char line[1024];
	int ln = 0;
	uint32_t held = 0; // keys held during "wait" (set by "keys" command)
	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	while (fgets(line, sizeof(line), f)) {
		++ln;
		char* hash = strchr(line, '#');
		if (hash) *hash = '\0';
		char* tok[8] = {0};
		int nt = 0;
		for (char* t = strtok(line, " \t\r\n"); t && nt < 8; t = strtok(NULL, " \t\r\n")) tok[nt++] = t;
		if (!nt) continue;
		const char* cmd = tok[0];
		if (!strcasecmp(cmd, "wait")) {
			if (nt < 2) die(ln, "wait N");
			runFrames(atol(tok[1]), held);
		} else if (!strcasecmp(cmd, "press") || !strcasecmp(cmd, "hold")) {
			uint32_t k;
			if (nt < 2 || !parseKeys(tok[1], &k)) die(ln, "bad keys '%s'", nt > 1 ? tok[1] : "");
			long n = nt > 2 ? atol(tok[2]) : (!strcasecmp(cmd, "press") ? 2 : 1);
			long after = nt > 3 ? atol(tok[3]) : 0;
			runFrames(n, held | k);
			runFrames(after, held);
		} else if (!strcasecmp(cmd, "tap")) {
			// tap KEYS [COUNT] [GAP]: press KEYS for 2 frames, release for GAP (default 10) frames, COUNT times
			uint32_t k;
			if (nt < 2 || !parseKeys(tok[1], &k)) die(ln, "bad keys");
			long count = nt > 2 ? atol(tok[2]) : 1, gap = nt > 3 ? atol(tok[3]) : 10;
			for (long i = 0; i < count; ++i) { runFrames(2, held | k); runFrames(gap, held); }
		} else if (!strcasecmp(cmd, "keys")) {
			// keys KEYS|NONE: set keys held during subsequent waits
			if (nt < 2) die(ln, "keys KEYS|NONE");
			if (!strcasecmp(tok[1], "none")) held = 0;
			else if (!parseKeys(tok[1], &held)) die(ln, "bad keys");
		} else if (!strcasecmp(cmd, "shot") || !strcasecmp(cmd, "screenshot")) {
			if (nt < 2) die(ln, "shot NAME");
			screenshot(ln, tok[1]);
		} else if (!strcasecmp(cmd, "savestate")) {
			if (nt < 2) die(ln, "savestate NAME");
			saveState(ln, tok[1]);
		} else if (!strcasecmp(cmd, "loadstate")) {
			if (nt < 2) die(ln, "loadstate NAME|PATH");
			struct stat st;
			const char* p = tok[1];
			if (stat(p, &st) != 0) p = outPath(tok[1], ".ss0");
			loadState(ln, p);
		} else if (!strcasecmp(cmd, "reset")) {
			core->reset(core);
			printf("frame %lu: reset\n", frames);
		} else if (!strcasecmp(cmd, "read") || !strcasecmp(cmd, "peek")) {
			// read SYMBOL|ADDR [8|16|32]
			if (nt < 2) die(ln, "read SYMBOL [8|16|32]");
			uint32_t a;
			resolveAddr(ln, tok[1], &a);
			int sz = nt > 2 ? atoi(tok[2]) : 32;
			uint32_t v = sz == 8 ? core->busRead8(core, a) : sz == 16 ? core->busRead16(core, a) : core->busRead32(core, a);
			printf("frame %lu: read %s @0x%08X = 0x%X (%u)\n", frames, tok[1], a, v, v);
		} else if (!strcasecmp(cmd, "write") || !strcasecmp(cmd, "poke")) {
			// write SYMBOL|ADDR VALUE [8|16|32]
			if (nt < 3) die(ln, "write SYMBOL VALUE [8|16|32]");
			uint32_t a;
			resolveAddr(ln, tok[1], &a);
			uint32_t v = strtoul(tok[2], NULL, 0);
			int sz = nt > 3 ? atoi(tok[3]) : 32;
			if (sz == 8) core->busWrite8(core, a, v); else if (sz == 16) core->busWrite16(core, a, v); else core->busWrite32(core, a, v);
		} else if (!strcasecmp(cmd, "echo")) {
			printf("frame %lu:", frames);
			for (int i = 1; i < nt; ++i) printf(" %s", tok[i]);
			putchar('\n');
		} else if (!strcasecmp(cmd, "quit") || !strcasecmp(cmd, "exit")) {
			break;
		} else {
			die(ln, "unknown command '%s'", cmd);
		}
		fflush(stdout);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	double dt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
	printf("done: %lu frames in %.2fs (%.0f fps, %.1fx realtime)\n", frames, dt, frames / dt, frames / dt / 59.7275);
	if (f != stdin) fclose(f);
	core->deinit(core);
	free(vbuf);
	return 0;
}
