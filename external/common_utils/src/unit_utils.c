/* Implementation of unit conversion helpers (double precision) */
#include <math.h>
#include "../unit_utils.h"

#ifndef PI_D
#ifdef M_PI
#define PI_D M_PI
#else
#define PI_D 3.14159265358979323846264338327950288
#endif
#endif

/* conversion constants - use multiplication where possible */
static const double METER_TO_FOOT = 3.28083989501312335958; /* 1 / 0.3048 */
static const double FOOT_TO_METER = 0.3048;
static const double KILOMETER_TO_MILE = 0.62137119223733396962; /* 1000/1609.344 */
static const double MILE_TO_KILOMETER = 1.609344;
static const double KILOGRAM_TO_POUND = 2.2046226218487757; /* 1/0.45359237 */
static const double POUND_TO_KILOGRAM = 0.45359237;
static const double LITER_TO_GALLON = 0.2641720523581484; /* US liquid gallon */
static const double GALLON_TO_LITER = 3.785411784;
static const double SQUARE_METER_TO_SQUARE_FOOT = 10.76391041671;
static const double ACRE_TO_SQUARE_METER = 4046.8564224;
static const double MPS_TO_MPH = 2.2369362920544;
static const double MPS_TO_KMPH = 3.6;
static const double KMPH_TO_MPH = 0.62137119223733;
static const double PASCAL_TO_PSI = 0.00014503773773020923;
static const double PSI_TO_PASCAL = 6894.757293168;
static const double JOULE_TO_CALORIE = 0.2390057361376673; /* 1/4.184 */
static const double CALORIE_TO_JOULE = 4.184;
static const double JOULE_TO_KWH = 1.0 / 3600000.0;
static const double KWH_TO_JOULE = 3600000.0;
static const double WATT_TO_HP = 1.0 / 745.699872;
static const double HP_TO_WATT = 745.699872;

/* Length */
inline double meter_to_kilometer(double meters) { return meters * 0.001; }
inline double kilometer_to_meter(double kilometers) { return kilometers * 1000.0; }
inline double meter_to_foot(double meters) { return meters * METER_TO_FOOT; }
inline double foot_to_meter(double feet) { return feet * FOOT_TO_METER; }
inline double kilometer_to_mile(double kilometers) { return kilometers * KILOMETER_TO_MILE; }
inline double mile_to_kilometer(double miles) { return miles * MILE_TO_KILOMETER; }
inline double meter_to_mile(double meters) { return meters * (1.0 / 1609.344); }
inline double mile_to_meter(double miles) { return miles * 1609.344; }
inline double kilometer_to_foot(double kilometers) { return kilometer_to_meter(kilometers) * METER_TO_FOOT; }

/* Area */
inline double square_meter_to_square_foot(double m2) { return m2 * SQUARE_METER_TO_SQUARE_FOOT; }
inline double square_foot_to_square_meter(double ft2) { return ft2 / SQUARE_METER_TO_SQUARE_FOOT; }
inline double acre_to_square_meter(double acres) { return acres * ACRE_TO_SQUARE_METER; }
inline double square_meter_to_acre(double m2) { return m2 / ACRE_TO_SQUARE_METER; }

/* Mass */
inline double kilogram_to_gram(double kg) { return kg * 1000.0; }
inline double gram_to_kilogram(double g) { return g * 0.001; }
inline double kilogram_to_pound(double kg) { return kg * KILOGRAM_TO_POUND; }
inline double pound_to_kilogram(double lb) { return lb * POUND_TO_KILOGRAM; }

/* Temperature */
inline double celsius_to_fahrenheit(double c) { return (c * 9.0 / 5.0) + 32.0; }
inline double fahrenheit_to_celsius(double f) { return (f - 32.0) * 5.0 / 9.0; }
inline double celsius_to_kelvin(double c) { return c + 273.15; }
inline double kelvin_to_celsius(double k) { return k - 273.15; }
inline double fahrenheit_to_kelvin(double f) { return celsius_to_kelvin(fahrenheit_to_celsius(f)); }
inline double kelvin_to_fahrenheit(double k) { return celsius_to_fahrenheit(kelvin_to_celsius(k)); }

/* Volume */
inline double liter_to_cubic_meter(double liters) { return liters * 0.001; }
inline double cubic_meter_to_liter(double m3) { return m3 * 1000.0; }
inline double liter_to_gallon(double liters) { return liters * LITER_TO_GALLON; }
inline double gallon_to_liter(double gallons) { return gallons * GALLON_TO_LITER; }

/* Velocity / Speed */
inline double mps_to_kmph(double mps) { return mps * MPS_TO_KMPH; }
inline double kmph_to_mps(double kmph) { return kmph / MPS_TO_KMPH; }
inline double mps_to_mph(double mps) { return mps * MPS_TO_MPH; }
inline double mph_to_mps(double mph) { return mph / MPS_TO_MPH; }
inline double kmph_to_mph(double kmph) { return kmph * KMPH_TO_MPH; }
inline double mph_to_kmph(double mph) { return mph / KMPH_TO_MPH; }

/* Pressure */
inline double pascal_to_psi(double pa) { return pa * PASCAL_TO_PSI; }
inline double psi_to_pascal(double psi) { return psi * PSI_TO_PASCAL; }
inline double pascal_to_bar(double pa) { return pa * 1e-5; }
inline double bar_to_pascal(double bar) { return bar * 100000.0; }
inline double pascal_to_atm(double pa) { return pa / 101325.0; }
inline double atm_to_pascal(double atm) { return atm * 101325.0; }

/* Energy */
inline double joule_to_calorie(double j) { return j * JOULE_TO_CALORIE; }
inline double calorie_to_joule(double cal) { return cal * CALORIE_TO_JOULE; }
inline double joule_to_kwh(double j) { return j * JOULE_TO_KWH; }
inline double kwh_to_joule(double kwh) { return kwh * KWH_TO_JOULE; }

/* Power */
inline double watt_to_horsepower(double w) { return w * WATT_TO_HP; }
inline double horsepower_to_watt(double hp) { return hp * HP_TO_WATT; }

/* Angle */
inline double degree_to_radian(double deg) { return deg * (PI_D / 180.0); }
inline double radian_to_degree(double rad) { return rad * (180.0 / PI_D); }

/* Data sizes */
inline double byte_to_kibibyte(double bytes) { return bytes / 1024.0; }
inline double kibibyte_to_byte(double kib) { return kib * 1024.0; }
inline double byte_to_kilobyte(double bytes) { return bytes / 1000.0; }
inline double kilobyte_to_byte(double kb) { return kb * 1000.0; }

