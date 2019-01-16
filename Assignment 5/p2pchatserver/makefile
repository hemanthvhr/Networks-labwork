all:	p2pchatserver.cpp
	g++ -o chatserver -std=c++11 p2pchatserver.cpp -w
clean: 
	rm chatserver
brun:
	-rm chatserver
	make all
	@read -p "Enter the user name: " user;	\
	./chatserver $$user;
run:
	@read -p "Enter the user name: " user;	\
	./chatserver $$user;
