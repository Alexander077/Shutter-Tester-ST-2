#pragma once

#include <Arduino.h>
#include "Common.h"
#include "MeasuredResult.h"

#define MEASUREMENT_SERIES_MAX_SIZE 10
#define SERIES_SUMMARY_FIELDS_PER_PAGE 5
#define SERIES_SUMMARY_PAGES_COUNT 2

// Available series sizes (1 = single measurement, current behavior)
const uint8_t measurementSeriesOptions[] = {1, 2, 3, 5, 7, 10};
const uint8_t measurementSeriesOptionsCount = sizeof(measurementSeriesOptions) / sizeof(measurementSeriesOptions[0]);

struct MeasurementSeries
{
	uint8_t totalCount;  // how many measurements the series consists of
	uint8_t storedCount; // how many results are actually stored in results[]
	MeasuredResult results[MEASUREMENT_SERIES_MAX_SIZE];
};

// Fields of MeasuredResult that take part in the series statistics and charts
enum class SeriesField : uint8_t
{
	SENSOR0_TIME = 0,
	SENSOR1_TIME,
	CURTAIN1_SPAN_A_TIME,
	CURTAIN1_SPAN_A_SPEED,
	CURTAIN1_TOTAL_TIME,
	CURTAIN2_SPAN_A_TIME,
	CURTAIN2_SPAN_A_SPEED,
	CURTAIN2_TOTAL_TIME,
	SLIT_WIDTH_SENSOR0,
	SLIT_WIDTH_SENSOR1,
	COUNT
};

#define SERIES_FIELDS_COUNT ((uint8_t)SeriesField::COUNT)
#define SERIES_REPORT_PAGES_COUNT (SERIES_SUMMARY_PAGES_COUNT + SERIES_FIELDS_COUNT)

inline double getSeriesFieldValue(const MeasuredResult &res, SeriesField field)
{
	switch (field)
	{
	case SeriesField::SENSOR0_TIME:
		return res.sensor0Time;
	case SeriesField::SENSOR1_TIME:
		return res.sensor1Time;
	case SeriesField::CURTAIN1_SPAN_A_TIME:
		return res.curtain1spanAtime;
	case SeriesField::CURTAIN1_SPAN_A_SPEED:
		return res.curtain1spanAspeed;
	case SeriesField::CURTAIN1_TOTAL_TIME:
		return res.curtain1TotalTime;
	case SeriesField::CURTAIN2_SPAN_A_TIME:
		return res.curtain2spanAtime;
	case SeriesField::CURTAIN2_SPAN_A_SPEED:
		return res.curtain2spanAspeed;
	case SeriesField::CURTAIN2_TOTAL_TIME:
		return res.curtain2TotalTime;
	case SeriesField::SLIT_WIDTH_SENSOR0:
		return res.slitWidthSensor0;
	case SeriesField::SLIT_WIDTH_SENSOR1:
		return res.slitWidthSensor1;
	default:
		return 0;
	}
}

// Special (sentinel) values of MeasuredResult are negative, real measurements are always positive
inline bool isSeriesFieldValueValid(double value)
{
	return value > 0;
}

struct SeriesFieldStats
{
	double avg;
	double min;
	double max;
	uint8_t validCount;
};

inline void computeSeriesFieldStats(const MeasurementSeries &series, SeriesField field, SeriesFieldStats &stats)
{
	double sum = 0;
	stats.avg = 0;
	stats.min = 0;
	stats.max = 0;
	stats.validCount = 0;

	for (uint8_t i = 0; i < series.storedCount; i++)
	{
		double value = getSeriesFieldValue(series.results[i], field);

		if (!isSeriesFieldValueValid(value))
		{
			continue;
		}

		if (stats.validCount == 0)
		{
			stats.min = value;
			stats.max = value;
		}
		else
		{
			if (value < stats.min)
			{
				stats.min = value;
			}

			if (value > stats.max)
			{
				stats.max = value;
			}
		}

		sum += value;
		stats.validCount++;
	}

	if (stats.validCount > 0)
	{
		stats.avg = sum / stats.validCount;
	}
}

struct SeriesFieldDescriptor
{
	const char *tableLabel; // short label for the summary table row
	const char *chartTitle; // title of the chart page
	bool displayAsSpeed;	// sensor time fields are displayed as shutter speed 1/X on charts
};

const SeriesFieldDescriptor seriesFieldDescriptors[SERIES_FIELDS_COUNT] =
	{
		{"S1 time ms", "Sensor 1 speed", true},
		{"S2 time ms", "Sensor 2 speed", true},
		{"C1 span ms", "Curtain 1 span time", false},
		{"C1 spd m/s", "Curtain 1 speed", false},
		{"C1 tot ms", "Curtain 1 total time", false},
		{"C2 span ms", "Curtain 2 span time", false},
		{"C2 spd m/s", "Curtain 2 speed", false},
		{"C2 tot ms", "Curtain 2 total time", false},
		{"Slit S1 mm", "Slit width sensor 1", false},
		{"Slit S2 mm", "Slit width sensor 2", false},
};
