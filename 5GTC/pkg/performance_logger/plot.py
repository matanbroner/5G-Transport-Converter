# Plotting functions for the tc_server_py package

import matplotlib.pyplot as plt
from matplotlib.figure import Figure
import numpy as np
import argparse


def reject_outliers(data, m=2):
    return data[abs(data - np.mean(data)) < m * np.std(data)]


def calculate_throughput(values, interval_ms=None):
    """
    Calculate the throughput from a list of values
    given the interval between each value in milliseconds
    """
    tp = []
    for i in range(1, len(values)):
        tp.append(values[i] - values[i - 1])
    return tp


def plot_subflow_features(
    data,
    features="all",
    fx_time=False,
    iteration_ms=500,
    subflow_keys=None,
    xlabel=None,
    ylabel=None,
    title=None,
):
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
    features = (
        list(data[list(data.keys())[0]].keys()) if features == "all" else features
    )
    for feature in features:
        # Create a new figure for each feature
        fig = Figure()
        ax = fig.add_subplot(111)
        # Get subflow data for the feature
        subflow_data = [subflow[feature] for subflow in data.values()]

        number_iterations = max([len(subflow) for subflow in subflow_data])

        # If fx_time, plot the x axis as time (in minutes)
        if fx_time:
            iterations_per_minute = (60 * 1000) / iteration_ms
            #  Number of ticks = (ms per iteration) * (number of iterations) all divided by 60,000 ms
            num_minutes = ((len(number_iterations) * iteration_ms) / 1000) / 60
            # Round up to the nearest minute
            num_minutes = int(np.ceil(num_minutes))
            ticks_loc = np.arange(
                0, len(number_iterations) + iterations_per_minute, iterations_per_minute
            )
            ax.set_xticks(ticks_loc)
            ax.set_xticklabels(np.arange(0, num_minutes + 1, 1))
            ax.set_xlabel(xlabel if xlabel else "Time (minutes)")
        else:
            ax.set_xlabel(xlabel if xlabel else "Iteration")
        # Plot the data
        for i in range(len(subflow_data)):
            # Left pad the data with 0s so that all subflows have the same length
            subflow_data[i] = np.pad(
                subflow_data[i], (0, number_iterations - len(subflow_data[i])), "constant"
            )
            ax.plot(subflow_data[i])
        # Set the title and labels
        ax.set_title(title if title else feature)
        ax.set_ylabel(ylabel if ylabel else feature)
        # Add a legend
        if subflow_keys:
            ax.legend(subflow_keys)
        else:
            ax.legend(data.keys())
        figs[feature] = fig
    return figs


def plot_transfer(subflow_data, iteration_ms=500, subflow_keys=None):
    """
    Plot the transfer rate of a subflow
    """
    # Create a new figure
    fig = Figure()
    ax = fig.add_subplot(111)
    for subflow in subflow_data:
        # Calculate the throughput
        iterations_per_minute = (60 * 1000) / iteration_ms
        #  Number of ticks = (ms per iteration) * (number of iterations) all divided by 60,000 ms
        num_minutes = ((len(subflow_data[subflow]["tcpi_bytes_acked"]) * iteration_ms) / 1000) / 60
        # Round up to the nearest minute
        num_minutes = int(np.ceil(num_minutes))
        ticks_loc = np.arange(
            0, len(subflow_data[subflow]["tcpi_bytes_acked"]) + iterations_per_minute, iterations_per_minute
        )
        tp = calculate_throughput(subflow_data[subflow]["tcpi_bytes_acked"])
        ax.set_xticks(ticks_loc)
        ax.set_xticklabels(np.arange(0, num_minutes + 1, 1))
        ax.set_xlabel("Time (minutes)")
        # Plot the throughput
        ax.plot(tp)
    # Set the title and labels
    ax.set_title("Subflow Transfer Rate")
    ax.set_xlabel("Time (minutes)")
    ax.set_ylabel("Bytes")

    ax.legend(subflow_keys if subflow_keys else subflow_data.keys())
    return fig
