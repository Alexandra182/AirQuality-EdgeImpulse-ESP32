#include <air-quality-esp32_inferencing.h>
#include "SparkFunCCS811.h"

#define FREQUENCY_HZ        EI_CLASSIFIER_FREQUENCY
#define INTERVAL_MS         (1000 / (FREQUENCY_HZ + 1))
#define CCS811_ADDR         0x5B

CCS811 airQualitySensor(CCS811_ADDR);

static unsigned long last_interval_ms = 0;
// to classify 1 frame of data you need EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE values
float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
// keep track of where we are in the feature array
size_t feature_ix = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Started");

  // Initialize air quality sensor
  Wire.begin();

  if (airQualitySensor.begin() == false) {
    Serial.print("CCS811 error. Please check wiring. Freezing...");
    while (1)
      ;
  }
}

void loop() {
  if (millis() > last_interval_ms + INTERVAL_MS) {
    last_interval_ms = millis();

    if (airQualitySensor.dataAvailable()) {

      // read sensor data in exactly the same way as in the Data Forwarder example
      airQualitySensor.readAlgorithmResults();
      // fill the features buffer
      features[feature_ix] = airQualitySensor.getCO2();
      Serial.println(String(feature_ix) + " | " + String(features[feature_ix]));
      feature_ix++;
    }

    // features buffer full? then classify!
    if (feature_ix == EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
      ei_impulse_result_t result;

      // create signal from features frame
      signal_t signal;
      numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
      Serial.println("passed");
      // run classifier
      EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
      Serial.println("classifier");

      ei_printf("run_classifier returned: %d\n", res);
      if (res != 0) return;

      // print predictions
      ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
                result.timing.dsp, result.timing.classification, result.timing.anomaly);

      // print the predictions
      for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("%s:\t%.5f\n", result.classification[ix].label, result.classification[ix].value);
      }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
      ei_printf("anomaly:\t%.3f\n", result.anomaly);
#endif

      // reset features frame
      feature_ix = 0;
    }
  }
}

void ei_printf(const char *format, ...) {
  static char print_buf[1024] = { 0 };

  va_list args;
  va_start(args, format);
  int r = vsnprintf(print_buf, sizeof(print_buf), format, args);
  va_end(args);

  if (r > 0) {
    Serial.write(print_buf);
  }
}
