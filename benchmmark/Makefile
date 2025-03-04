include docker-compose/.env
include .env

USER := $(shell whoami)
DOCKER_PROJ_NAME := ${DOCKER_PROJ_PREFIX}_airu
ALICE_STATS_PID := .alice_stats.pid
BOB_STATS_PID := .bob_stats.pid


.PHONY: start-docker clean analysis

default: all

clean:
	@[ -d "$(PWD)/docker-compose/logs" ] && rm -rf $(PWD)/docker-compose/logs && echo "Directory removed." || echo "Directory not exists."
	@[ -d "$(PWD)/docker-compose/tmp_data" ] && rm -rf $(PWD)/docker-compose/tmp_data && echo "Directory removed." || echo "Directory not exists."
	@[ -f "$(PWD)/$(ALICE_STATS_PID)" ] && @kill `cat $(ALICE_STATS_PID)` || true && rm -f "$(PWD)/$(ALICE_STATS_PID)"
	@[ -f "$(PWD)/$(BOB_STATS_PID)" ] && @kill `cat $(BOB_STATS_PID)` || true && rm -f "$(PWD)/$(BOB_STATS_PID)"
	@(docker compose -p ${DOCKER_PROJ_NAME} down)

all: clean init start-docker analysis
	@echo "well done!"

init:
	go install github.com/tsliwowicz/go-wrk@latest

analysis:
	@[ ! -d "$(PWD)/docker-compose/logs" ] && mkdir -p "$(PWD)/docker-compose/logs" && echo "Directory created." || echo "Directory already exists."
	@[ ! -d "$(PWD)/docker-compose/logs/alice" ] && mkdir -p "$(PWD)/docker-compose/logs/alice" && echo "Directory created." || echo "Directory already exists."
	@[ ! -d "$(PWD)/docker-compose/logs/bob" ] && mkdir -p "$(PWD)/docker-compose/logs/bob" && echo "Directory created." || echo "Directory already exists."

	@nohup python $(PWD)/stats.py $(DOCKER_PROJ_NAME)-serving_alice-1 $(PWD)/docker-compose/logs/alice/stats.csv 2>&1 & echo $$! > $(PWD)/$(ALICE_STATS_PID)
	@nohup python $(PWD)/stats.py $(DOCKER_PROJ_NAME)-serving_bob-1 $(PWD)/docker-compose/logs/bob/stats.csv 2>&1 & echo $$! > $(PWD)/$(BOB_STATS_PID)

	~/go/bin/go-wrk -c $(CONCURRENCY_NUM) -d $(TEST_DURATION) -body @$(PWD)/request_body/$(BODY_JSON) -M POST -H "Content-Type:application/json" http://127.0.0.1:9010/PredictionService/Predict > $(PWD)/docker-compose/logs/wrk.log

	python $(PWD)/plot_csv_data.py $(PWD)/docker-compose/logs/alice/stats.csv $(PWD)/docker-compose/logs/alice
	python $(PWD)/plot_csv_data.py $(PWD)/docker-compose/logs/bob/stats.csv $(PWD)/docker-compose/logs/bob
	docker logs $(DOCKER_PROJ_NAME)-serving_bob-1 > $(PWD)/docker-compose/logs/bob/serving.log
	docker logs $(DOCKER_PROJ_NAME)-serving_alice-1 > $(PWD)/docker-compose/logs/alice/serving.log

	@[ -f "$(PWD)/$(ALICE_STATS_PID)" ] && kill `cat $(ALICE_STATS_PID)` || true && rm -f "$(PWD)/$(ALICE_STATS_PID)"
	@[ -f "$(PWD)/$(BOB_STATS_PID)" ] && kill `cat $(BOB_STATS_PID)` || true && rm -f "$(PWD)/$(BOB_STATS_PID)"
	@(docker compose -p ${DOCKER_PROJ_NAME} down)

start-docker:
	@(cd $(PWD)/docker-compose && docker compose -p ${DOCKER_PROJ_NAME} up -d)
	sleep 15
