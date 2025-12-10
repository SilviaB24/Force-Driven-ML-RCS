#!/bin/bash

MODE=$1


# Default values
DEBUG=0
FEAT_S=0
FEAT_P=0


# Check if help must be displayed
if [ "$MODE" == "-h" ] || [ "$MODE" == "--help" ] || [ -z "$MODE" ]; then
    echo "Usage: ./run_code.bash [mode] [options]"
    echo ""
    echo "Modes:"
    echo "  run                Compile and run the scheduler."
    echo "  check [file]       Compile and run the checker on the specified file."
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
                ;;
            -featS)
                FEAT_S=1
                ;;
            -featP)
                FEAT_P=1
                ;;
        esac
    done

    # Compile the scheduler
    g++ -std=c++17 LSMain.cpp LS.cpp readInputs.cpp -o scheduler

    # Run the scheduler
    if [ $? -eq 0 ]; then

        ./scheduler "$DEBUG" "$FEAT_S" "$FEAT_P"
    fi

    exit 0
elif [ "$MODE" == "check" ]; then

    # Compile the checker
    g++ -std=c++17 -I. checker.cpp -o checker_mac         

    # Run the checker
    if [ $? -eq 0 ]; then

        CHECKER_FILE_NAME=$2
        ./checker_mac $CHECKER_FILE_NAME
    fi
    
else
    echo "Invalid mode. Use -h or --help for usage information."
    exit 1
fi

