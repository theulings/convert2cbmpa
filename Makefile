all :
	g++ convert2cbmpa.cpp `Magick++-config --cppflags --cxxflags --ldflags --libs` -o convert2cbmpa

depends :
	apt install nlohmann-json3-dev libmagick++-dev

install :
	cp convert2cbmpa /usr/bin/
