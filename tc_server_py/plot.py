# Plotting functions for the tc_server_py package

import matplotlib.pyplot as plt
import numpy as np
import json
import sys
import os
import argparse
import logging
import time

def reject_outliers(data, m=1):
    return data[abs(data - np.mean(data)) < m * np.std(data)]

def plot_subflow_features(data, output_file=None):
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
    print("Plotting subflow features for {} subflows".format(len(data)))
    # Get all features
    features = list(data[list(data.keys())[0]].keys())
    for feature in features:
        # Get subflow data for the feature
        subflow_data = [subflow[feature] for subflow in data.values()]
        # Reject the outliers for each subflow
        subflow_data = [reject_outliers(np.array(subflow)) for subflow in subflow_data]
        # Plot the data
        for s_data in subflow_data:
            # Plot the data as a line of new color
            plt.plot(s_data)

        plt.title(feature)
        plt.xlabel("Iteration")
        plt.ylabel(feature)
        plt.legend(data.keys())
        if output_file is None:
            plt.show()
        else:
            plt.savefig(output_file)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot the data from a performance log")
    parser.add_argument("filename", help="The filename of the performance log to plot")
    parser.add_argument("--output_file", help="The filename to save the plot to")
    args = parser.parse_args()

    # Read the data from the performance log
    with open(args.filename, "r") as f:
        data = json.loads(f.read())

    # Plot the data
    plot_subflow_features(data, args.output_file)