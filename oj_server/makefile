USE_MYSQL ?= 0

oj_server:OJ_server.cpp
	@if [ "$(USE_MYSQL)" = "1" ]; then \
		echo "[build] USE_MYSQL=1"; \
		g++ -o $@ $^ -I./include -L./lib -std=c++11 -lpthread -lctemplate -ljsoncpp -lmysqlclient; \
	else \
		echo "[build] USE_MYSQL=0 (file-based questions)"; \
		g++ -o $@ $^ -std=c++11 -lpthread -lctemplate -ljsoncpp; \
	fi
.PHONY:clean
clean:
	rm	-f	oj_server