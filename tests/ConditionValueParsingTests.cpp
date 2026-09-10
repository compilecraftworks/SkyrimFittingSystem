#include "conditions/ValueParsing.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

void Expect(bool value, const char *message) {
  if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int main() {
  using namespace sfs::conditions;
  Expect(TryParseInt("  +0012\t") == 12, "signed/zero-padded integer remains valid");
  Expect(TryParseInt("-2147483648") == std::numeric_limits<std::int32_t>::min(), "int32 minimum");
  Expect(TryParseInt("2147483647") == std::numeric_limits<std::int32_t>::max(), "int32 maximum");
  for (auto text : {"", " ", "12abc", "12 34", "1.5", "1e2", "2147483648", "-2147483649", "+", "++1", "+-1", "+ 1", "0x10"}) {
    Expect(!TryParseInt(text), text);
  }
  Expect(!TryParseInt(std::string("12\0garbage", 10)), "integer embedded NUL");
  Expect(TryParseFloat(" +1.25 ") == 1.25f, "positive float");
  Expect(TryParseFloat("-2.5e2") == -250.0f, "negative/scientific float");
  Expect(TryParseFloat(".5") == 0.5f, "fraction without leading zero");
  Expect(TryParseFloat("0x1p2") == 4.0f, "prior native hexadecimal float syntax retained");
  Expect(TryParseFloat("0") == 0.0f, "zero is a valid value, not parse failure");
  for (auto text : {"", " ", "12abc", "1.2x", "1e", "1 2", "NaN", "nan(payload)", "inf", "-Infinity", "1e40", "1e400", "1e-400"}) {
    Expect(!TryParseFloat(text), text);
  }
  Expect(!TryParseFloat(std::string("1.5\0tail", 8)), "float embedded NUL");
  Expect(ParseAxisArgument("X") == 0 && ParseAxisArgument(" y ") == 1 &&
         ParseAxisArgument("z") == 2, "all axes and lowercase/whitespace remain valid");
  for (auto text : {"", "Q", "XY", "Zjunk", "2", "1"}) { Expect(!ParseAxisArgument(text), text); }
  Expect(!ParseAxisArgument(std::string("Z\0", 2)), "axis embedded NUL");
  Expect(ParseActorValueArgument("Health") == 24, "canonical ActorValue name");
  Expect(ParseActorValueArgument(" health\t") == 24, "uppercase fallback retained");
  Expect(ParseActorValueArgument("aggression") == 0, "ActorValue index zero is valid");
  Expect(ParseActorValueArgument("runtimeextension") == 16384, "engine success is not bounded by a compile-time enum");
  for (auto text : {"", "NoSuchActorValue", "None", "-1", "24", "HealthOops", "Health extra"}) {
    Expect(!ParseActorValueArgument(text), text);
  }
  Expect(!ParseActorValueArgument(std::string("Health\0tail", 11)), "ActorValue embedded NUL");
  std::cout << "Condition production numeric/axis/ActorValue parsing tests passed\n";
}
