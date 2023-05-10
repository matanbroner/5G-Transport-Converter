import os
from pkg.performance_logger.plot import *
from pkg.performance_logger.db import DB

FIGURES_DIR = os.path.join(os.path.dirname(__file__), "figures")
DB_NAME = "performance_log.db"
ITERATION_TIME_MS = 500


def plot_subflow_info_from_db(
    iteration_id, field, xlabel=None, ylabel=None, title=None
):
    """
    Plot the subflow info from a session_id
    """
    is_transfer_rate = field == "transfer_rate"
    db = DB({"db_path": DB_NAME, "delete_db_on_exit": False})
    subflows = db.read_all_subflows(iteration_id)
    key = "tcpi_bytes_acked" if is_transfer_rate else field
    data = {sf[1]: {key: []} for sf in subflows}
    for sf in subflows:
        data[sf[1]][key] = [
            row[0]
            for row in db.read_subflow_tcp_conn_info(iteration_id, sf[1], [field if not is_transfer_rate else "tcpi_bytes_acked"])
        ]
    if is_transfer_rate:
        fig = plot_transfer(data, iteration_ms=ITERATION_TIME_MS, subflow_keys=["WiFi", "LTE"])
    else:
        figs = plot_subflow_features(
            data,
            [field],
            fx_time=True,
            iteration_ms=ITERATION_TIME_MS,
            subflow_keys=["WiFi", "LTE"],
            ylabel=ylabel,
            xlabel=xlabel,
            title=title,
        )
        fig = figs[field]
    # Export the figure to a png
    fig.savefig(
        os.path.join(FIGURES_DIR, "{}_{}.png".format(iteration_id, field))
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot the data from a performance log")
    parser.add_argument("--iteration_id", type=str, help="The iteration id to plot")
    parser.add_argument("--field", type=str, help="The field to plot")
    parser.add_argument("--xlabel", type=str, help="The x-axis label", default=None)
    parser.add_argument("--ylabel", type=str, help="The y-axis label", default=None)
    parser.add_argument("--title", type=str, help="The title of the plot", default=None)
    args = parser.parse_args()

    if not os.path.exists(FIGURES_DIR):
        os.makedirs(FIGURES_DIR)
    plot_subflow_info_from_db(args.iteration_id, args.field, args.xlabel, args.ylabel, args.title)
