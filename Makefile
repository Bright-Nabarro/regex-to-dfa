build:
	cd src && \
	g++ -g -Wall -Wextra -Wpedantic -Wconversion -std=c++20 *.cpp -o regex-to-dfa && \
	mv regex-to-dfa ..

run: build
	./regex-to-dfa

clang-format:
	clang-format -i src/*
