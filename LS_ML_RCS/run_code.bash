#!/bin/bash

MODE=$1


# Default values
DEBUG=0
FEAT_S=0
FEAT_P=0


# Define colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color


# Check if help must be displayed
if [ "$MODE" == "-h" ] || [ "$MODE" == "--help" ] || [ -z "$MODE" ]; then
    echo -e "${CYAN}Usage: ./run_code.bash [mode] [options]${NC}"
    echo ""
    echo "Modes:"
    echo -e "  ${GREEN}run${NC}                Compile and run the scheduler."
    echo -e "  ${GREEN}check [file]${NC}       Compile and run the checker on the specified file."
    echo ""
    echo "Options for 'run' mode:"
    echo "  -debug            Enable debug mode."
    echo "  -featS            Enable feature S for priority calculation."
    echo "  -featP            Enable feature P for priority calculation."
    exit 0
fi


if [ "$MODE" == "run" ]; then

    # Parse optional arguments
    for arg in "${@:2}"; do
        case $arg in
            -debug)
                DEBUG=1
                echo -e "${YELLOW}[INFO] Debug mode enabled.${NC}"
                ;;
            -featS)
                FEAT_S=1
                echo -e "${YELLOW}[INFO] Feature S enabled.${NC}"
                ;;
            -featP)
                FEAT_P=1
                echo -e "${YELLOW}[INFO] Feature P enabled.${NC}"
                ;;
        esac
    done

    # Compile the scheduler
    echo -e "${CYAN}[BUILD] Compiling scheduler...${NC}"
    g++ -std=c++17 LSMain.cpp LS.cpp readInputs.cpp -o scheduler

    # Run the scheduler
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}[BUILD] Compilation successful. Running scheduler...${NC}"
        ./scheduler "$DEBUG" "$FEAT_S" "$FEAT_P"
    else
        echo -e "${RED}[ERROR] Compilation failed.${NC}"
        exit 1
    fi

    exit 0

elif [ "$MODE" == "check" ]; then

    # Compile the checker
    echo -e "${CYAN}[BUILD] Compiling checker...${NC}"
    g++ -std=c++17 -I. checker.cpp -o checker_mac         

    # Run the checker
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}[BUILD] Checker compiled. Running checks...${NC}"
        CHECKER_FILE_NAME=$2
        if [ -z "$CHECKER_FILE_NAME" ]; then
            echo -e "${RED}[ERROR] No file specified for checking.${NC}"
            exit 1
        fi
        ./checker_mac $CHECKER_FILE_NAME
    else 
        echo -e "${RED}[ERROR] Checker compilation failed.${NC}"
        exit 1
    fi
    
else
    echo -e "${RED}[ERROR] Invalid mode. Use -h or --help for usage information.${NC}"
    exit 1
fi

