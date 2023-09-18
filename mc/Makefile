INCS=-I${FILAMENT_HOME}/include -I/usr/include/SDL2
LIBS=-L${FILAMENT_HOME}/lib -L/usr/lib/SDL2
LINK=-lm -lfilament -lSDL2 -lbackend -lbasis_transcoder -lblueg1 -lbluevk -lcamutils -lcivetweb -ldracodec -lfilamat -lshaders -lutils -lfilamentapp -limage -lstb -lfilaflat -lfilabridge

TARGET_NAME=mc
TARGET_SRCS=mc.cpp

all:
	$(CXX) $(TARGET_SRCS) -o ${TARGET_NAME} $(INCS) $(LIBS) -lfilament -lm -std=c++17
