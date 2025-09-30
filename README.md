# uefa-draw

A 2024/25+ UEFA Champions League, Europa League, and Conference League draw
simulator, multithreaded and written in C++. Data analysis and visualization in
Python.

## Overview

In the 2024/25 season, UEFA's European soccer/football competitions replaced the
group stage with a **league phase**, where all 36 clubs compete in a single
table to qualify for knockouts.

In the league phase, each club plays 8 random opponents (6 in Conference
League), determined by a draw at the beginning of the season. The opponents are
subject to the following country restrictions:

1. Clubs cannot play other clubs from their own country.
2. Clubs cannot play more than 2 clubs from the same country.

These restrictions make the matchup probabilities non-uniform, and difficult to
analyze mathematically. This project allows users to estimate the matchup
probabilities by running many draw simulations in parallel. Code is initially
based on [`inker/draw`](https://github.com/inker/draw), which is an interactive
TypeScript implementation of a single draw, designed for the web.

## In this repo

- `src/` - C++ source code
- `drivers/` - C++ driver code
  - `main.cpp`: simulation driver (runs many simulations in parallel with output
    suppressed)
  - `debug.cpp`: debug driver (runs single simulation with full output
    displayed)
- `include/` - third-party C++ libraries
- `licenses/` - licenses for the third-party C++ libraries
- `data/` - teams and actual draw results per competition, per year
- `examples/` - example simulation results and visualizations from past seasons
- `scripts/` - Python scripts for populating `data/` and generating
  visualizations

## Getting started

### Requirements

- gcc 8+
- Python 3.12+

### Setup

```shell
# Clone repo
git clone https://github.com/evxiong/uefa-draw.git

cd uefa-draw/scripts

# Create a new venv
python -m venv .venv

# Activate venv
source .venv/Scripts/activate

# Install requirements
pip install -r requirements.txt

cd ..
```

## Usage

### Monte Carlo simulation

`make all` creates the `main` executable, which simulates many draws.

```shell
$ make all
$ ./bin/main <year> <competition> <iterations> [<input teams csv path> <output results csv path>]
```

- `<year>` is the earlier year of a season; ex. `2025` represents the 2025/26
  season
- `<competition>` can be `ucl` (Champions League), `uel` (Europa League), or
  `uecl` (Conference League)
- `<iterations>` is the number of simulations to run
- the default input teams csv path is `data/<year>/<competition>/teams.csv`
- the default output results csv path is
  `results/<competition>_<year>_<iterations>_<YYYYMMDD>_<HHMMSS>.csv` where
  `<YYYYMMDD>` and `<HHMMSS>` represent the system date and time at the start of
  execution, respectively

#### Retrieving draw data

To automatically add teams and draw results for a particular year and
competition to `data/`, run `scripts/scrape.py`.

```shell
$ cd scripts
$ python scrape.py <year> <competition>
```

- `<year>` is the earlier year of a season; ex. `2025` represents the 2025/26
  season
- `<competition>` can be `ucl` (Champions League), `uel` (Europa League), or
  `uecl` (Conference League)
- teams will be placed in `data/<year>/<competition>/teams.csv` and draw results
  in `data/<year>/<competition>/draw.txt`

#### Visualizing results

To visualize simulation results as a heatmap of matchup probabilities, run
`scripts/analysis.py`.

```shell
$ cd scripts
$ python analysis.py <path to results csv>
```

- visualizations will be placed in
  `results/<competition>_<year>_<iterations>_<YYYYMMDD>_<HHMMSS>.png`

#### Interpretation of results

This program runs many simulations to estimate each matchup's probability. For a
95% confidence interval with a sample size of $n=25000$, the maximum margin of
error (assuming maximum variance) on any estimated matchup probability is
$\plusmn 0.0062$, or $\plusmn 0.62\%$. Since $p$ will almost never be $0.5$ in
this case, the margin of error will almost always be smaller than this value.
For example, if a particular matchup occurs in the sampled simulations 25% of
the time, then we are 95% confident that the interval $25 \plusmn 0.54\%$
contains the true matchup probability.

### Single simulation

`make DRIVER=debug` creates the `debug` executable, which simulates 1 draw at a
time with full output.

```shell
$ make DRIVER=debug
$ ./bin/debug <year> <competition> [<initial picked matches txt path>]
```

- by default, no matches are initially picked; this can be used to initialize
  the draw state for debugging purposes (no constraint checks are performed on
  these matches); the txt file must have a format where each line represents a
  single match, in the form `<home team abbrev>-<away team abbrev>`, such as
  `TOT-BAR`.

### Cleanup

`make clean` removes `bin` and `build` directories.
