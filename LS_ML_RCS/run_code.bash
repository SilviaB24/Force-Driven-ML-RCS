#!/bin/bash

# IMPLEMENTED BY SILVIA
MODE=$1


# Default values
DEBUG=0
FEAT_S=0
FEAT_P=0
DATA_TYPE="invdelay"

# Define colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Detect if running in Windows without WSL or Git Bash
if [[ "$OS" == "Windows_NT" && -z "$MSYSTEM" && -z "$WSL_DISTRO_NAME" ]]; then
    echo -e "\033[0;31m[ERROR]\033[0m This script must be run in a Bash-compatible terminal."
    echo "Try one of these options:"
    echo " - Open Git Bash (recommended)"
    echo " - Or run inside Windows Subsystem for Linux (WSL)"
    exit 1
fi

# Extension for executables (.exe on Windows, empty on Linux/mac)
EXT=""
if [[ "$OS" == "Windows_NT" ]]; then
    EXT=".exe"
fi


# Check if help must be displayed
if [ "$MODE" == "-h" ] || [ "$MODE" == "--help" ] || [ -z "$MODE" ]; then
    echo -e "${CYAN}Usage: ./run_code.bash [mode] [options]${NC}"
    echo ""
    echo "Modes:"
    echo -e "  ${GREEN}run${NC}                Compile and run the scheduler."
    echo -e "  ${GREEN}check${NC}       Compile and run the checker on the specified file."
    echo ""
    echo "Options for 'run' mode:"
    echo -e " ${YELLOW}--debug${NC}            Enable debug mode."
    echo -e " ${YELLOW}--featS${NC}            Enable feature S for priority calculation."
    echo -e " ${YELLOW}--featP${NC}            Enable feature P for priority calculation."
    echo -e " ${YELLOW}--uniform${NC}          Run with UNIFORM distribution (default is InvDelay)."
    echo -e " ${YELLOW}--invdelay${NC}         Run with INVERSE DELAY distribution."
    echo ""
    echo "Options for 'check' mode:"
    echo -e " ${YELLOW}[file]${NC}             Input file to check (single file for check or CSV for report generation)."
    echo -e " ${YELLOW}--debug${NC}            Enable debug mode."
    exit 0
fi


# Parse optional arguments
for arg in "${@:2}"; do
    case $arg in
        --debug|-D)
            DEBUG=1
            echo -e "${YELLOW}[INFO] Debug mode enabled.${NC}"
            ;;
        --featS|-S)
            FEAT_S=1
            echo -e "${YELLOW}[INFO] Feature S enabled.${NC}"
            ;;
        --featP|-P)
            FEAT_P=1
            echo -e "${YELLOW}[INFO] Feature P enabled.${NC}"
            ;;
        --uniform|-U)
            DATA_TYPE="uniform"
            echo -e "${YELLOW}[INFO] Mode set to: UNIFORM distribution.${NC}"
            ;;
        --invdelay|-I)
            DATA_TYPE="invdelay"
            echo -e "${YELLOW}[INFO] Mode set to: INVERSE DELAY distribution.${NC}"
            ;;
    esac
done
    

if [ "$MODE" == "run" ]; then

    # Compile the scheduler
    echo -e "${CYAN}[BUILD] Compiling scheduler...${NC}"
    echo ""
    g++ -std=c++17 -O3 -I. LSMain.cpp LS.cpp FDS.cpp readInputs.cpp -o scheduler

    # Run the scheduler
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}[BUILD] Compilation successful. Running scheduler...${NC}"
        ./scheduler$EXT "$DEBUG" "$FEAT_S" "$FEAT_P" "$DATA_TYPE"
    else
        echo -e "${RED}[ERROR] Compilation failed.${NC}"
        exit 1
    fi

    exit 0

elif [ "$MODE" == "check" ]; then

    # Compile the checker
    echo -e "${CYAN}[BUILD] Compiling checker...${NC}"
    echo ""
    g++ -std=c++17 -I. checker.cpp -o checker_mac         

    # Run the checker
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}[BUILD] Checker compiled. Running checks...${NC}"

        INPUT_FILE=$2

        if [ -z "$INPUT_FILE" ]; then
            echo -e "${RED}[ERROR] Missing input file.${NC}"
            exit 1
        fi

        if [[ "$INPUT_FILE" == *.csv ]]; then

            # If CSV file is provided a report will be generated
            echo -e "${CYAN}[REPORT] Generating Excel report from $INPUT_FILE...${NC}"
            if [ -f "run_checker.py" ]; then
                python3 run_checker.py "$INPUT_FILE" "./checker_mac" "$DEBUG"
            else
                echo -e "${RED}[ERROR] run_checker.py not found.${NC}"
                exit 1
            fi

        else
            # If single file is provided simply check that file
            if [ -f "$INPUT_FILE" ]; then
                echo -e "${CYAN}[CHECK] Verifying single file: $INPUT_FILE...${NC}"
                ./checker_mac$EXT "$INPUT_FILE"
            else
                echo -e "${RED}[ERROR] Input file not found.${NC}"
                exit 1
            fi

        fi

    else 
        echo -e "${RED}[ERROR] Checker compilation failed.${NC}"
        exit 1
    fi
    
else
    echo -e "${RED}[ERROR] Invalid mode. Use -h or --help for usage information.${NC}"
    exit 1
fi


# END IMPLEMENTED BY SILVIA