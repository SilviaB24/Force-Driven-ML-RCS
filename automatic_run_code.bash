#!/bin/bash

# Define colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color


RUN=0
CHECK=0

for arg in "$@"; do
    case $arg in
        --run|-R)
            RUN=1
            ;;
        --check|-C)
            CHECK=1
            ;;
        --help|-h)
            echo -e "${CYAN}Usage: ./automatic_run_code.bash [options]${NC}"
            echo ""
            echo "Options:"
            echo -e "  ${YELLOW}--run, -R${NC}         Execute the automatic run sequence."
            echo -e "  ${YELLOW}--check, -C${NC}       Execute the automatic check sequence."
            echo -e "  ${YELLOW}--help, -h${NC}        Display this help message."
            echo ""
            exit 0
            ;;
    esac
done



# for i in {1.0,0.9,0.8,0.7,0.6,0.5,0.4,0.3,0.2,0.1}
for i in {0.05,0.025,0.01}
do 

    # AUTOMATIC RUNNING
    if [ $RUN -eq 1 ]; then
        echo -e "\n${CYAN}[AUTO RUN] Executing with scaling factor: $i${NC}\n"

        echo -e "${YELLOW}[1/6]${NC} Uniform - LS standard implementation"
        ./run_code.bash run -U -B -F=$i

        echo -e "${YELLOW}[2/6]${NC} InvDelay - LS standard implementation"
        ./run_code.bash run -B -F=$i

        echo -e "${YELLOW}[3/6]${NC} Uniform - LS without Feature S and P"
        ./run_code.bash run -U -F=$i

        echo -e "${YELLOW}[4/6]${NC} InvDelay - LS without Feature S and P"
        ./run_code.bash run -F=$i

        echo -e "${YELLOW}[5/6]${NC} Uniform - LS with Feature S and P"
        ./run_code.bash run -U -S -P -F=$i

        echo -e "${YELLOW}[6/6]${NC} InvDelay - LS with Feature S and P"
        ./run_code.bash run -S -P -F=$i
    fi


    # AUTOMATIC CHECKING

    if [ $CHECK -eq 1 ]; then
      
        TARGET_DIR=$(printf "CSV/%.2f" $i)

        echo -e "${CYAN}[AUTO CHECK] Scanning directory: $TARGET_DIR${NC}"

        if [ -d "$TARGET_DIR" ]; then

            # Find all CSV files in the directory
            for res_file in "$TARGET_DIR"/*.csv; do

                # Ensure the glob expanded to an actual file
                if [ -f "$res_file" ]; then

                    echo -e " -> Checking file: $res_file"
                    ./run_code.bash check "$res_file"
                else
                    echo -e "${YELLOW}[WARN] No CSV files found in $TARGET_DIR${NC}"
                fi
            done
        else
            echo -e "${RED}[ERROR] Directory $TARGET_DIR does not exist!${NC}"
        fi
    fi

done
