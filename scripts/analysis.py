"""
Generate heatmap visualization.

Usage:
$ python analysis.py <path to results csv>

Visualization will be placed in
`results/<competition>_<year>_<iterations>_<YYYYMMDD>_<HHMMSS>.png`.
"""

import argparse
import csv
import datetime
import os
from io import BytesIO
from typing import cast

import matplotlib.pyplot as plt
import numpy as np
import requests
import yaml
from matplotlib.axes import Axes
from matplotlib.colors import LinearSegmentedColormap
from matplotlib.offsetbox import AnnotationBbox, OffsetImage
from PIL import Image
from shared import HEADERS, Competition

CACHE_DIR = ".cache/logos"


COLORS = {
    Competition.UCL: "#00004b",
    Competition.UEL: "#993f00",
    Competition.UECL: "#005809",
}


Value = int | float | bool | str | None


def convert_csv_value(val: str) -> Value:
    """Convert CSV string value to Python value.

    Args:
        val (str): CSV value to convert

    Returns:
        Value: `None`, `int`, `float`, `bool`, or `str`.
    """
    val = val.strip()
    if val == "":
        return None

    try:
        return int(val)
    except ValueError:
        pass

    try:
        return float(val)
    except ValueError:
        pass

    if val.lower() in ("true", "false"):
        return val.lower() == "true"

    return val


def parse_csv(path: str) -> tuple[dict[str, Value], list[dict[str, Value]]]:
    """Parse CSV with frontmatter.

    Args:
        path (str): path to CSV

    Returns:
        tuple[dict[str, Value], list[dict[str, Value]]]: (frontmatter dict, list
        of dicts containing data)
    """
    frontmatter = {}
    data = []
    with open(path, encoding="utf-8") as fd:
        first_line = fd.readline()
        if first_line.strip() == "---":
            fm_lines: list[str] = []
            for line in fd:
                if line.strip() == "---":
                    break
                fm_lines.append(line)
            frontmatter = yaml.safe_load("".join(fm_lines)) or {}
        else:
            fd.seek(0)

        reader = csv.DictReader(fd)
        for row in reader:
            data.append({k: convert_csv_value(v) for k, v in row.items()})
    return frontmatter, data


def get_team_logo(team_id: str, force_refresh: bool = False) -> Image.Image:
    """Retrieve image of team logo from `CACHE_DIR` or UEFA website.

    If not present in `CACHE_DIR`, retrieve team logo from UEFA website and
    store it in cache.

    Args:
        team_id (str): UEFA team id
        force_refresh (bool, optional): if True, always fetch from UEFA website;
            otherwise, use cache if present. Defaults to False.

    Returns:
        Image.Image: `PIL.Image.Image` object
    """
    path = os.path.join(CACHE_DIR, f"{team_id}.png")
    os.makedirs(os.path.dirname(path), exist_ok=True)

    if not force_refresh and os.path.exists(path):
        return Image.open(path).convert("RGBA")

    url = f"https://img.uefa.com/imgml/TP/teams/logos/32x32/{team_id}.png"

    r = requests.get(url, headers=HEADERS)
    r.raise_for_status()

    with open(path, "wb") as fd:
        fd.write(r.content)

    return Image.open(BytesIO(r.content)).convert("RGBA")


def get_competition_logo(competition: Competition) -> Image.Image:
    """Retrieve image of competition logo from `data/img`.

    Args:
        competition (Competition): current competition

    Returns:
        Image.Image: `PIL.Image.Image` object
    """
    return Image.open(f"../data/img/{competition.value}.png").convert("RGBA")


def render_heatmap(
    data: np.ndarray,
    row_labels: list[str],
    col_labels: list[str],
    ax: Axes,
    colorbar_label: str,
    val_fmt: str = "{:.1f}",
    threshold: float = 25,
    **kwargs,
):
    """Create heatmap.

    Args:
        data (np.ndarray): 2D numpy array
        row_labels (list[str]): string labels for rows
        col_labels (list[str]): string labels for columns
        ax (Axes): `Axes` to which heatmap is plotted
        colorbar_label (str, optional): colorbar label
        val_fmt (str): format for data values. Defaults to "{:.1f}".
        threshold (float): values above threshold will be displayed in
            white; otherwise, black. Defaults to 25.
        **kwargs: arguments forwarded to `imshow`
    """
    # Plot the heatmap
    im = ax.imshow(data, **kwargs)

    # Create colorbar
    cbar = ax.figure.colorbar(im, ax=ax, aspect=50, pad=0.025)
    cbar.ax.tick_params(labelsize=6)
    cbar.ax.set_ylabel(colorbar_label, rotation=-90, fontsize=6.5, va="bottom")

    # Show all ticks with labels
    ax.set_xticks(np.arange(data.shape[1]), labels=col_labels)
    ax.set_yticks(np.arange(data.shape[0]), labels=row_labels)

    # Set horizontal tick labels to appear on top of axis
    ax.tick_params(top=True, bottom=False, labeltop=True, labelbottom=False)

    # Draw grid lines
    ax.set_xticks(np.arange(data.shape[1] + 1) - 0.5, minor=True)
    ax.set_yticks(np.arange(data.shape[0] + 1) - 0.5, minor=True)
    ax.grid(which="minor", color="0.5", linestyle="-", linewidth=0.5)
    ax.tick_params(which="minor", bottom=False, left=False)

    third_breaks = [2.5, 5.5, 11.5, 14.5, 20.5, 23.5, 29.5, 32.5]
    for p in third_breaks:
        ax.axvline(x=p, color="#444444", linewidth=0.75)
        ax.axhline(y=p, color="#444444", linewidth=0.75)

    pot_breaks = [8.5, 17.5, 26.5]
    for p in pot_breaks:
        ax.axvline(x=p, color="black", linewidth=1)
        ax.axhline(y=p, color="black", linewidth=1)

    # Label each cell with value
    for i in range(data.shape[0]):
        for j in range(data.shape[1]):
            if i != j and data[i, j]:
                im.axes.text(
                    j,
                    i,
                    val_fmt.format(data[i, j]),
                    horizontalalignment="center",
                    verticalalignment="center",
                    fontsize="5",
                    color=("white" if data[i, j] > threshold else "black"),
                )


def render_team_logo(team_ind: int, team_id: str, ax: Axes):
    """Add team logo to horizontal and vertical axes.

    Args:
        team_ind (int): team index (0-indexed)
        team_id (str): UEFA team id
        ax (Axes): heatmap `Axes` object
    """
    img = get_team_logo(team_id)
    im = OffsetImage(np.array(img), zoom=0.34375)
    im.image.axes = ax

    # render on vertical axis
    ab = AnnotationBbox(
        im,
        (0, team_ind),
        xybox=(-36.0, 0.0),
        frameon=False,
        xycoords="data",
        boxcoords="offset points",
        pad=0,
    )

    ax.add_artist(ab)

    # render on horizontal axis
    ab = AnnotationBbox(
        im,
        (team_ind, 0),
        xybox=(0.0, 32.0),
        frameon=False,
        xycoords="data",
        boxcoords="offset points",
        pad=0,
    )

    ax.add_artist(ab)


def render_competition_logo(competition: Competition, ax: Axes):
    """Add competition logo to top left corner of heatmap.

    Args:
        competition (Competition): current competition
        ax (Axes): heatmap `Axes` object
    """
    img = get_competition_logo(competition)
    im = OffsetImage(np.array(img), zoom=0.375)  # assumes 64x64 image
    im.image.axes = ax

    ab = AnnotationBbox(
        im,
        (0, 0),
        xybox=(-32.0, 28.0),
        frameon=False,
        xycoords="data",
        boxcoords="offset points",
        pad=0,
    )

    ax.add_artist(ab)


def create_colormap(competition: Competition, color: str) -> LinearSegmentedColormap:
    """Create `Colormap` that transitions from white to the given hex `color`.

    Args:
        competition (Competition): current competition
        color (str): hex color, ex. "#111111"

    Returns:
        LinearSegmentedColormap: `Colormap` object
    """
    return LinearSegmentedColormap.from_list(
        f"{competition}_cmap", [(0, "#ffffff"), (0.2, "#ffffff"), (1, color)]
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("path", type=str)
    args = parser.parse_args()

    frontmatter, data = parse_csv(args.path)
    competition = Competition(frontmatter["competition"])
    year = cast(int, frontmatter["year"])
    iterations = cast(int, frontmatter["simulations"])
    timestamp = cast(datetime.datetime, frontmatter["timestamp"])

    _, teams = parse_csv(f"../data/{year}/{competition.value}/teams.csv")
    team_abbrevs: list[str] = cast(list[str], [team["abbrev"] for team in teams])
    team_ids: list[str] = [str(team["id"]) for team in teams]
    num_teams = len(teams)

    probs = np.zeros((num_teams, num_teams))

    for row in data:
        t1 = cast(int, row["t1"])
        t2 = cast(int, row["t2"])
        total = cast(int, row["total"])
        probs[t1][t2] = (total / iterations) * 100
        probs[t2][t1] = (total / iterations) * 100

    fig, ax = plt.subplots(figsize=(10, 8), dpi=300)

    render_heatmap(
        data=probs,
        row_labels=team_abbrevs,
        col_labels=team_abbrevs,
        ax=ax,
        colorbar_label="Matchup Probability (%)",
        cmap=create_colormap(competition, COLORS[competition]),
    )

    for team_ind, team_id in enumerate(team_ids):
        render_team_logo(team_ind, team_id, ax)

    render_competition_logo(competition, ax)

    plt.yticks(fontname="monospace", fontsize=6.5)
    plt.xticks(fontname="monospace", fontsize=6.5)

    ax.text(
        -0.07,
        1.08,
        f"{year}/{(year % 100) + 1} {competition.value.upper()} Draw Probabilities, League Phase\n",
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontsize=7,
        linespacing=1.5,
    )
    ax.text(
        -0.07,
        1.08,
        f"n={iterations}, generated {timestamp}",
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontsize=6.5,
        color="#444444",
    )

    fig.tight_layout()
    plt.savefig(
        f"../results/{competition.value}_{year}_{iterations}_{timestamp.strftime('%Y%m%d')}_{timestamp.strftime('%H%M%S')}.png",
        bbox_inches="tight",
    )


if __name__ == "__main__":
    main()
