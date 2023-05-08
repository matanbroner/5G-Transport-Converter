# Web UI for the performance logger
# Serve a webpage that allows the user to view the performance metrics

import logging
import base64
from multiprocessing import Process
from io import BytesIO
from flask import Flask, render_template, Response

# Local imports
from performance_logger.db import DB
from performance_logger.plot import plot_subflow_features, plot_transfer

logger = logging.getLogger("webui")
logger.setLevel(logging.DEBUG)

app = Flask(__name__, template_folder="templates", static_folder="webui/static")

features = ["tcpi_rtt", "tcpi_bytes_sent"]

# Main route: makes API calls to get the data for the plots
@app.route("/<iteration_id>")
def iteration_page(iteration_id):
    return render_template("plots.html", iteration_id=iteration_id)

@app.route("/api/plots/<iteration_id>")
def data(iteration_id):
    # Get the database
    db = DB({
        "db_path": "performance_log.db",
        "delete_db_on_exit": False
    })
    # Get all subflows for the iteration
    subflows = db.read_all_subflows(iteration_id)
    # Get the data for each subflow
    data = {}
    for subflow in subflows:
        # Get the TCP connection info for the subflow
        # Also get the tcpi_bytes_acked for throughput calculation
        _features = features + ["tcpi_bytes_acked"]
        data[subflow[1]] = { feature : [] for feature in _features }
        tcpi = db.read_subflow_tcp_conn_info(iteration_id, subflow[1], _features)
        for row in tcpi:
            for i, feature in zip(range(len(_features)), _features):
                data[subflow[1]][feature].append(row[i])
    figs = plot_subflow_features(data)
    transfer_fig = plot_transfer(data)
    figs["transfer"] = transfer_fig
    return_tags = []
    for feature, fig in figs.items():
        buf = BytesIO()
        fig.savefig(buf, format="png")
        # fig.savefig("{}.png".format(feature), format="png")
        buf.seek(0)
        data = base64.b64encode(buf.getbuffer()).decode("ascii")
        return_tags.append({
            "feature": feature,
            "data": f"<img src='data:image/png;base64,{data}'/>"
        })
    return { "plots": return_tags }



# Run the web server in a new process
def run_webui(host, port):
    # Disable logging for the web server
    log = logging.getLogger('werkzeug')
    log.disabled = True

    app.run(host=host, port=port, debug=False)

class WebUI:
    __instance = None
    def __new__(cls, *args, **kwargs):
        if WebUI.__instance is None:
            WebUI.__instance = object.__new__(cls)
            # Get host and port from args
            WebUI.__instance.host = args[0]
            WebUI.__instance.port = args[1]
            if "log" in kwargs:
                logger.setLevel(kwargs["log"])
        return WebUI.__instance

    def __del__(self):
        self.stop()

    def log_iteration_page(self, iteration_id):
        logger.info("WebUI running on http://{}:{}/{}".format(self.host, self.port, iteration_id))
    
    def run(self):
        self.process = Process(target=run_webui, args=(self.host, self.port,))
        self.process.start()

    def stop(self):
        self.process.terminate()
        self.process.join()



