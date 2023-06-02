.PHONY: help

help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'

setup:  ## Per machine setup
	cd project && ./install-ubuntu.sh
	cd project/pid_controller/ && rm -rf rpclib && git clone https://github.com/rpclib/rpclib.git
	pip3 install pandas matplotlib

sim:  ## Run Carla Simulator
	su student -s /bin/bash -c 'SDL_VIDEODRIVER=offscreen cd /opt/carla-simulator/ && ./CarlaUE4.sh -opengl'

controller:  ## Make the controller completely
	cd project/pid_controller/ && cmake . && make  # (This last command compiles your c++ code, run it after every change in your code)

cfix:  ## Recompile only the controller (quicker)
	cd project/pid_controller/ && make

crun:  ## Run controller
	cd project && ./run_main_pid.sh

eval:  ## Create evaluation
	cd project && python3 plot_pid.py
