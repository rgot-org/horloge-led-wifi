#pragma once

// Stub minimal de MLP::ExponentialFilter, utilisé uniquement pour satisfaire
// la dépendance interne "Filter.h" de EasyButtonTouch (mode bouton tactile),
// que nous n'utilisons pas ici (bouton poussoir classique sur BP_PIN).
// La bibliothèque officielle (MegunoLink) ne compile pas sur ce toolchain
// ESP32-S3 (conflit de surcharges uint32_t / unsigned long).
template <typename T>
class ExponentialFilter {
public:
  ExponentialFilter(int weight, T initial) : _weight(weight), _current(initial) {}

  void Filter(T value) {
    _current = ((100 - _weight) * (long)_current + _weight * (long)value) / 100;
  }

  T Current() const {
    return _current;
  }

private:
  int _weight;
  T _current;
};
