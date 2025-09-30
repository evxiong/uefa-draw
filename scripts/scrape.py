"""
Scrape teams and draw results from UEFA website.

Usage:
$ python scrape.py <year> <competition>

Teams will be placed in `data/<year>/<competition>/teams.csv` and draw results
in `data/<year>/<competition>/draw.txt`.
"""

import argparse
import csv
import os

import requests
from shared import HEADERS, Competition


def get_params_and_key(competition: Competition, year: int) -> tuple[dict, str]:
    """Get UEFA request params and object key.

    Args:
        competition (Competition): current competition
        year (int): earlier year of season (ex. 2025 for 2025/26 season)

    Returns:
        tuple[dict, str]: (request params, object key)
    """
    title_resource_key = (
        "draw_title_UCL_LP"
        if competition == Competition.UCL
        else (
            "draw_title_UEL_LP"
            if competition == Competition.UEL
            else "draw_title_UECL_LP"
        )
    )

    params = {
        "seasonYear": year,
        "competitionId": (
            1
            if competition == Competition.UCL
            else 14
            if competition == Competition.UEL
            else 2019
            # ucl: 1, uel: 14, uecl: 2019
        ),
        "optionalFields": "GROUPS,POTS,ROUNDS,TEAMS",
    }

    return params, title_resource_key


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("year", type=int)
    parser.add_argument("competition", type=Competition, choices=list(Competition))

    args = parser.parse_args()
    competition: Competition = args.competition
    season: int = args.year

    params, title_resource_key = get_params_and_key(competition, season + 1)
    r = requests.get(
        "https://fsp-draw-service.uefa.com/v1/draws", params=params, headers=HEADERS
    )
    d = r.json()
    data = next((x for x in d if x["titleResourceKey"] == title_resource_key))

    # write to teams.csv
    teams = []
    teams_by_id = {}

    for i, pot in enumerate(data["rounds"][0]["pots"]):
        for team in pot["teams"]:
            teams_by_id[team["id"]] = {
                "pot": i + 1,
                "abbrev": team["teamCode"],
                "country": team["countryCode"],
                "team": team["internationalName"],
                "id": team["id"],
            }

        for team_id in pot["teamIds"]:
            t = teams_by_id[team_id]
            teams.append(t)

    # create dirs if dne
    path = f"../data/{season}/{competition.value}/teams.csv"
    os.makedirs(os.path.dirname(path), exist_ok=True)

    # csv: pot,abbrev,country,team,id
    with open(path, "w", newline="", encoding="utf-8") as fd:
        writer = csv.DictWriter(fd, fieldnames=teams[0].keys())
        writer.writeheader()
        writer.writerows(teams)

    # write to draw.txt
    seen_matchups = set()
    games = []

    for t in data["rounds"][0]["result"]["groups"][0]["teamSlots"]:
        t_id = t["teamIds"][0]
        for opp in t["opponents"]:
            if (t_id + ":" + opp["teamId"] in seen_matchups) or (
                opp["teamId"] + ":" + t_id in seen_matchups
            ):
                continue
            if opp["location"] == "HOME":
                games.append(
                    teams_by_id[t_id]["abbrev"]
                    + "-"
                    + teams_by_id[opp["teamId"]]["abbrev"]
                )
            else:
                games.append(
                    teams_by_id[opp["teamId"]]["abbrev"]
                    + "-"
                    + teams_by_id[t_id]["abbrev"]
                )
            seen_matchups.add(t_id + ":" + opp["teamId"])

    # create dirs if dne
    path = f"../data/{season}/{competition.value}/draw.txt"
    os.makedirs(os.path.dirname(path), exist_ok=True)

    with open(path, "w", encoding="utf-8") as fd:
        fd.writelines(g + "\n" for g in games)


if __name__ == "__main__":
    main()
