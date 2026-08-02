#ifndef UNIT_UTILS_H
#define UNIT_UTILS_H

/* Unit conversion utilities
 * Conversions are grouped by physical dimension. Functions accept and
 * return `float` values. Only sensible conversions are provided.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Length (meters, kilometers, miles, feet) ---- */
double meter_to_kilometer(double meters);
double kilometer_to_meter(double kilometers);
double meter_to_foot(double meters);
double foot_to_meter(double feet);
double kilometer_to_mile(double kilometers);
double mile_to_kilometer(double miles);
double meter_to_mile(double meters);
double mile_to_meter(double miles);
double kilometer_to_foot(double kilometers);

/* ---- Area ---- */
double square_meter_to_square_foot(double m2);
double square_foot_to_square_meter(double ft2);
double acre_to_square_meter(double acres);
double square_meter_to_acre(double m2);

/* ---- Mass (kilograms, grams, pounds) ---- */
double kilogram_to_gram(double kg);
double gram_to_kilogram(double g);
double kilogram_to_pound(double kg);
double pound_to_kilogram(double lb);
static inline double kilo_to_lb(double kg) { return kilogram_to_pound(kg); }

/* ---- Temperature (Celsius, Fahrenheit, Kelvin) ---- */
double celsius_to_fahrenheit(double c);
double fahrenheit_to_celsius(double f);
double celsius_to_kelvin(double c);
double kelvin_to_celsius(double k);
double fahrenheit_to_kelvin(double f);
double kelvin_to_fahrenheit(double k);

/* ---- Volume (liters, cubic meters, US gallons) ---- */
double liter_to_cubic_meter(double liters);
double cubic_meter_to_liter(double m3);
double liter_to_gallon(double liters); /* US liquid gallon */
double gallon_to_liter(double gallons);

/* ---- Velocity / Speed ---- */
double mps_to_kmph(double mps);
double kmph_to_mps(double kmph);
double mps_to_mph(double mps);
double mph_to_mps(double mph);
double kmph_to_mph(double kmph);
double mph_to_kmph(double mph);

/* ---- Pressure ---- */
double pascal_to_psi(double pa);
double psi_to_pascal(double psi);
double pascal_to_bar(double pa);
double bar_to_pascal(double bar);
double pascal_to_atm(double pa);
double atm_to_pascal(double atm);

/* ---- Energy ---- */
double joule_to_calorie(double j);
double calorie_to_joule(double cal);
double joule_to_kwh(double j);
double kwh_to_joule(double kwh);

/* ---- Power ---- */
double watt_to_horsepower(double w);
double horsepower_to_watt(double hp);

/* ---- Angle ---- */
double degree_to_radian(double deg);
double radian_to_degree(double rad);

/* ---- Data sizes ---- */
double byte_to_kibibyte(double bytes);
double kibibyte_to_byte(double kib);
double byte_to_kilobyte(double bytes); /* decimal KB */
double kilobyte_to_byte(double kb);

#ifdef __cplusplus
}
#endif

/* Implementation include when requested */
#ifdef UNIT_UTILS_IMPLEMENTATION
#include "src/unit_utils.c"
#endif

#endif /* UNIT_UTILS_H */