CXX=clang++
WSCPP_DIR=./websocketpp/

all : ws_proxy.cpp
	$(CXX) -o ws_proxy ws_proxy.cpp -I $(WSCPP_DIR) -I /opt/local/include/ -L /opt/local/lib/ -lboost_system-mt -std=c++11 -D_WEBSOCKETPP_CPP11_STL_ 

clean : 
	rm ws_proxy