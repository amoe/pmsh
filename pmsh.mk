# XXX temporary hack until new headers are properly installed
xtra = ../libprojectM

run: pmsh.elf
	./pmsh.elf

pmsh.elf: pmsh.o pulse.o
	g++ -o pmsh.elf \
  pmsh.o pulse.o $$(pkg-config --libs libprojectM sdl libpulse) -lGL

pmsh.o: pmsh.cc pmsh.hh
	g++ -g -c -Wall -I $(xtra) pmsh.cc

pulse.o: pulse.cc pulse.hh
	g++ -g -c -Wall -I $(xtra) pulse.cc

clean:
	rm pmsh.o pulse.o pmsh.elf
