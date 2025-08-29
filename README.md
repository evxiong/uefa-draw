# uefa-draw

A 2024/25+ UEFA Champions League, Europa League, and Conference League draw
simulator, written in C++. Data analysis and visualization in Python.

## In this repo

- `src/` - C++ source code
- `drivers/` - C++ driver code
  - `main.cpp`: simulation driver (most outputs suppressed)
  - `debug.cpp`: run single simulation with full output displayed
- `data/` - team and draw data, scraping and analysis scripts
  - `yyyy/`
    - `teams/` - teams per competition
    - `draw/` - actual draw results per competition
  - `scrape.py` - retrieves team and draw data
- `analysis/` -
  - `sim/` (untracked) - simulation results per competition
  - `viz/` (untracked) - visualization images of simulation results
  - `analysis.py` - generates visualizations

## Requirements

- Python 3.12+
- g++

## Usage

- `make all`
- `make clean`

`./bin/main ucl 100 ./data/2025/teams/ucl.csv ./analysis/sim/ucl.csv`
