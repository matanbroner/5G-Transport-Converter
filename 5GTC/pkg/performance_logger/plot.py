# Plotting functions for the tc_server_py package

import matplotlib.pyplot as plt
from matplotlib.figure import Figure
import numpy as np
import json
import sys
import os
import argparse
import logging
import time

def reject_outliers(data, m=2):
    return data[abs(data - np.mean(data)) < m * np.std(data)]

def calculate_throughput(values, interval_ms=None):
    """
    Calculate the throughput from a list of values 
    given the interval between each value in milliseconds
    """
    tp = []
    for i in range(1, len(values)):
        tp.append(values[i] - values[i-1])
    return tp
    

def plot_subflow_features(data, features="all"):
    """
    Plot a series of features for two subflows
    Plot each feature as a separate subplot, each subflow as a separate line

    Data Format:
    {
        "subflow_id": {
            "feature_name": [feature_values]
        }
    }

    ex. 
    {
        "0": {
            "tcpi_rtt": [0.001, 0.002, 0.003]
        },
        "1": {
            "tcpi_rtt": [0.001, 0.002, 0.003]
        }
    }
    """
    # Get all features
    figs = {}
    features = list(data[list(data.keys())[0]].keys()) if features == "all" else features
    for feature in features:
        # Create a new figure for each feature
        fig = Figure()
        ax = fig.add_subplot(111)
        # Get subflow data for the feature
        subflow_data = [subflow[feature] for subflow in data.values()]
        # Reject the outliers for each subflow
        # subflow_data = [reject_outliers(np.array(subflow)) for subflow in subflow_data]
        # Plot the data
        for s_data in subflow_data:
            # Plot the data as a line of new color
            ax.plot(s_data)
        # Set the title and labels
        ax.set_title(feature)
        ax.set_xlabel("Iteration")
        ax.set_ylabel(feature)
        # Add a legend
        ax.legend(data.keys())
        
        figs[feature] = fig
    return figs

def plot_transfer(data):
    """
    Plot the transfer rate of a subflow
    """
    # Create a new figure
    fig = Figure()
    ax = fig.add_subplot(111)
    for subflow in data:
        # Calculate the throughput
        tp = calculate_throughput(data[subflow]["tcpi_bytes_acked"])
        # Plot the throughput
        ax.plot(tp)
    # Set the title and labels
    ax.set_title("Transfer")
    ax.set_xlabel("Iteration")
    ax.set_ylabel("Transfer Rate (bytes/500 ms)")

    ax.legend(data.keys())
    return fig


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot the data from a performance log")
    parser.add_argument("filename", help="The filename of the performance log to plot")
    parser.add_argument("--output_file", help="The filename to save the plot to")
    args = parser.parse_args()

    # Read the data from the performance log
    with open(args.filename, "r") as f:
        data = json.loads(f.read())

    # Plot the data
    plot_subflow_features(data, features=["tcpi_bytes_sent", "tcpi_bytes_received", "tcpi_bytes_acked", "tcpi_bytes_retrans", "tcpi_rtt"])
    plot_transfer(data)