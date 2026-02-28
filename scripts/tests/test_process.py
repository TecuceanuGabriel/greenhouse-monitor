import os
import sys
import tempfile

import pandas as pd
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from process import load_data, filter_by_timeframe, compute_medians, detect_outliers

SAMPLE_CSV = """\
Timestamp,Temperature (°C),Humidity (%),CH4 (ppm),CO2 (ppm)
2024-01-01 00:00:00,20,50,1.0,400
2024-01-01 06:00:00,22,55,1.2,420
2024-01-01 12:00:00,25,60,1.5,450
2024-01-01 18:00:00,23,58,1.3,430
2024-01-02 00:00:00,18,48,0.9,390
"""


@pytest.fixture
def sample_csv(tmp_path):
    path = tmp_path / "data.csv"
    path.write_text(SAMPLE_CSV)
    return str(path)


@pytest.fixture
def sample_df():
    df = pd.read_csv(pd.io.common.StringIO(SAMPLE_CSV), parse_dates=['Timestamp'])
    return df


class TestLoadData:
    def test_loads_csv(self, sample_csv):
        df = load_data(sample_csv)
        assert len(df) == 5
        assert 'Timestamp' in df.columns

    def test_parses_timestamps(self, sample_csv):
        df = load_data(sample_csv)
        assert pd.api.types.is_datetime64_any_dtype(df['Timestamp'])

    def test_column_names(self, sample_csv):
        df = load_data(sample_csv)
        expected = ['Timestamp', 'Temperature (°C)', 'Humidity (%)', 'CH4 (ppm)', 'CO2 (ppm)']
        assert list(df.columns) == expected


class TestFilterByTimeframe:
    def test_filters_range(self, sample_df):
        start = pd.Timestamp('2024-01-01 05:00:00')
        end = pd.Timestamp('2024-01-01 13:00:00')
        result = filter_by_timeframe(sample_df, start, end)
        assert len(result) == 2

    def test_returns_empty_for_no_match(self, sample_df):
        start = pd.Timestamp('2025-01-01 00:00:00')
        end = pd.Timestamp('2025-01-02 00:00:00')
        result = filter_by_timeframe(sample_df, start, end)
        assert len(result) == 0

    def test_full_range_returns_all(self, sample_df):
        start = pd.Timestamp('2023-01-01')
        end = pd.Timestamp('2025-01-01')
        result = filter_by_timeframe(sample_df, start, end)
        assert len(result) == 5


class TestComputeMedians:
    def test_median_values(self, sample_df):
        medians = compute_medians(sample_df)
        assert medians['Temperature (°C)'] == 22.0
        assert medians['Humidity (%)'] == 55.0
        assert abs(medians['CH4 (ppm)'] - 1.2) < 0.01
        assert medians['CO2 (ppm)'] == 420.0

    def test_single_row(self):
        df = pd.DataFrame({
            'Temperature (°C)': [42],
            'Humidity (%)': [77],
            'CH4 (ppm)': [2.5],
            'CO2 (ppm)': [600],
        })
        medians = compute_medians(df)
        assert medians['Temperature (°C)'] == 42


class TestDetectOutliers:
    def test_no_outliers_in_uniform_data(self, sample_df):
        outliers = detect_outliers(sample_df, 'Temperature (°C)')
        assert len(outliers) == 0

    def test_detects_outlier(self):
        df = pd.DataFrame({
            'Temperature (°C)': [20, 21, 22, 20, 21, 22, 20, 21, 100],
        })
        outliers = detect_outliers(df, 'Temperature (°C)')
        assert len(outliers) == 1
        assert outliers.iloc[0]['Temperature (°C)'] == 100

    def test_detects_low_outlier(self):
        df = pd.DataFrame({
            'Temperature (°C)': [20, 21, 22, 20, 21, 22, 20, 21, -50],
        })
        outliers = detect_outliers(df, 'Temperature (°C)')
        assert len(outliers) == 1
        assert outliers.iloc[0]['Temperature (°C)'] == -50
