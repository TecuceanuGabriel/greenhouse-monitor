import pandas as pd
from datetime import datetime

CSV_FILE = 'data_log.csv'

def load_data(file_path):
    return pd.read_csv(file_path, parse_dates=['Timestamp'])

def filter_by_timeframe(df, start_time, end_time):
    return df[(df['Timestamp'] >= start_time) & (df['Timestamp'] <= end_time)]

def compute_medians(df):
    return df[['Temperature (°C)', 'Humidity (%)', 'CH4 (ppm)', 'CO2 (ppm)']].median()

def detect_outliers(df, column):
    q1 = df[column].quantile(0.25)
    q3 = df[column].quantile(0.75)
    iqr = q3 - q1
    lower_bound = q1 - 1.5 * iqr
    upper_bound = q3 + 1.5 * iqr
    return df[(df[column] < lower_bound) | (df[column] > upper_bound)]

def main():
    filter_by_time_frame = input("Filter by time frame? (y/n)");

    df = load_data(CSV_FILE)
    df['Timestamp'] = pd.to_datetime(df['Timestamp'])

    res = df;

    if filter_by_time_frame == "y":
        start_str = input("Start time (YYYY-MM-DD HH:MM:SS): ")
        end_str = input("End time (YYYY-MM-DD HH:MM:SS): ")

        start_time = datetime.strptime(start_str, '%Y-%m-%d %H:%M:%S')
        end_time = datetime.strptime(end_str, '%Y-%m-%d %H:%M:%S')

        res = filter_by_timeframe(df, start_time, end_time)

        if res.empty:
            print("No data in the specified timeframe.")
            return

    print("\n-----Median Values:")
    medians = compute_medians(res)
    print(medians)

    print("\n-----Outliers:")
    for col in ['Temperature (°C)', 'Humidity (%)', 'CH4 (ppm)', 'CO2 (ppm)']:
        outliers = detect_outliers(res, col)
        if not outliers.empty:
            print(f"Outliers in {col}:\n")
            print(outliers[['Timestamp', col]])
        else:
            print(f"No outliers detected in {col}.")
        print("\n")

if __name__ == '__main__':
    main()
