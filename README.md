# Force-Driven-ML-RCS

This project implements a Force-Driven List Scheduling (FD-ML-RCS) algorithm along with baseline List Scheduling. It includes automation scripts for running experiments across various scaling factors, verifying results, and generating visualizations.

## Project Structure

| File/Folder | Description |
| :--- | :--- |
| **Source Code** | |
| `LSMain.cpp` | Main entry point for the scheduler. |
| `LS.cpp`, `LS.h` | Implementation of List Scheduling algorithms. |
| `FDS.cpp` | (Initial experimental) Implementation of Force-Directed Scheduling. |
| `ReadInputs.cpp` | Helper to parse DFG files and constraints. |
| `checker.cpp`, `checker.h` | (Changed) Verifier to validate scheduling results. |
| **Scripts** | |
| `run_code.bash` | **Main wrapper**. Compiles and runs the scheduler or checker for a single instance. |
| `automatic_run_code.bash` | **Batch Automation**. Runs experiments across all scaling factors (1.0 to 0.1) in all configurations. |
| `run_checker.py` | Python script to execute the checker and parse results into related CSVs. |
| `run_graphs.py` | Python script to generate visualization graphs from the output CSVs. |
| **Data & Configuration** | |
| `DFG/` | Folder containing input Data Flow Graphs (`.txt`). |
| `Constraints/` | Resource constraint files. |
| `lib_4type.txt` | Library file defining operation delays and resource types. |
| **Output** | |
| `CSV/` | Results stored as `.csv` files, organized by scaling factor. |
| `Graphs/` | Generated plots visualizing latency, runtime, and improvements. |
| `Results/` | Intermediate result storage, organized by scaling factor. |
| **Documentation** | |
| `README.md` | This documentation file. |
| `Docs/` | Slides and Report from Midterm, Slides from Final. |

-----

## Setup & Prerequisites

### 1\. Requirements

  * **C++ Compiler:** `g++`.
  * **Python 3:** Required for checker integration and graph generation.
  * **Bash Terminal:** Required to run the `.bash` automation scripts.

### 2\. Permissions

Ensure the scripts are executable:

```bash
chmod +x run_code.bash automatic_run_code.bash
```

### 3\. Environment

For python scripts use the provided virtual environment:

```bash
source venv/bin/activate
```

-----

## Manual Execution (`run_code.bash`)

Use this script to run a single scheduling experiment or verify a single CSV output file. It handles compilation automatically.

### Usage

```bash
./run_code.bash [mode] [options]
```

### Modes

  * **`run`**: Runs the scheduling algorithm.
  * **`check`**: Runs the verifier on a specific CSV result file.
  * **`--help` / `-h`**: Displays help information.

### Options for `run` Mode

| Flag | Description |
| :--- | :--- |
| `--base` / `-B` | Run the **Standard LS** Implementation (Default is enhanced with Force). |
| `--uniform` / `-U` | Use **Uniform** benchmark (Default is InvDelay). |
| `--invdelay` / `-I` | Use **Inverse Delay** distribution. |
| `--featS` / `-S` | Enable **Feature S** (resource constraints) for priorities. |
| `--featP` / `-P` | Enable **Feature P** (predecessors) for priorities. |
| `--scaling=X` / `-F=X` | Set **scaling factor** (e.g., `-F=0.5`). Default: 1.0. |
| `--debug` / `-D` | Enable verbose debug output. |

**Example:**

```bash
# Run Standard LS with Uniform distribution at 0.8 scaling
./run_code.bash run -U -B -F=0.8
```

### Options for `check` Mode

Pass the CSV file to verify as the second argument.

**Example:**

```bash
# Verify a specific result file
./run_code.bash check CSV/1.00/Results_LS_uniform.csv
```

-----

## Automatic Batch Execution (`automatic_run_code.bash`)

Use this script to reproduce the **full set of experiments**. It iterates through scaling factors from **1.0 down to 0.1**, for every configuration:

  * **Distributions:** Uniform, Inverse Delay.
  * **Algorithms:** Standard LS, FD-ML-RCS with No Features, FD-ML-RCS with Feature S & P.

### Usage

```bash
./automatic_run_code.bash [options]
```

### Options

| Flag | Description |
| :--- | :--- |
| `--run` / `-R` | **Execute Runs**: Runs 6 configurations per scaling factor (Uniform/InvDelay x Standard/NoFeat/FeatSP). |
| `--check` / `-C` | **Execute Checks**: Scans the `CSV/` directory and verifies every generated `.csv` file. |

**Example:**

```bash
# Run everything and check everything
./automatic_run_code.bash -R -C
```

-----

## Generating Graphs

To visualize the results after running the experiments, use the python script:

```bash
python3 run_graphs.py
```

This will generate plots in the `Graphs/` directory, comparing:

  * Latency Deltas (LS versions) vs Baseline (FALLS)
  * Runtime analysis.
  * Improvement percentages.

> **Note:** This project has been developed and tested on macOS. For running on Windows, it may be necessary to install the required tools via WSL or adapt the scripts accordingly.