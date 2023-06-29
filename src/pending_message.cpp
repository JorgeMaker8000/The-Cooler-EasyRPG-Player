#include "pending_message.h"
#include "game_variables.h"
#include "game_strings.h"
#include "game_actors.h"
#include "game_message.h"
#include "game_switches.h"
#include <lcf/data.h>
#include "output.h"
#include "utils.h"
#include "player.h"
#include "main_data.h"
#include <cassert>
#include <cctype>
#include <algorithm>

static void RemoveControlChars(std::string& s) {
	// RPG_RT ignores any control characters within messages.
	auto iter = std::remove_if(s.begin(), s.end(), [](const char c) { return Utils::IsControlCharacter(c); });
	s.erase(iter, s.end());
}

int PendingMessage::PushLineImpl(std::string msg) {
	RemoveControlChars(msg);
	msg = ApplyTextInsertingCommands(std::move(msg), Player::escape_char);
	texts.push_back(std::move(msg));
	return texts.size();
}

int PendingMessage::PushLine(std::string msg) {
	assert(!HasChoices());
	assert(!HasNumberInput());
	return PushLineImpl(std::move(msg));
}

int PendingMessage::PushChoice(std::string msg, bool enabled) {
	assert(!HasNumberInput());
	if (!HasChoices()) {
		choice_start = NumLines();
	}
	choice_enabled[GetNumChoices()] = enabled;
	return PushLineImpl(std::move(msg));
}

int PendingMessage::PushNumInput(int variable_id, int num_digits) {
	assert(!HasChoices());
	assert(!HasNumberInput());
	num_input_variable = variable_id;
	num_input_digits = num_digits;
	return NumLines();
}

void PendingMessage::PushPageEnd() {
	assert(!HasChoices());
	assert(!HasNumberInput());
	if (texts.empty()) {
		texts.push_back("");
	}
	texts.back().push_back('\f');
}

void PendingMessage::SetWordWrapped(bool value) {
	assert(texts.empty());
	word_wrapped = value;
}

void PendingMessage::SetChoiceCancelType(int value) {
	choice_cancel_type = value;
}

void PendingMessage::SetChoiceResetColors(bool value) {
	choice_reset_color = value;
}

std::string PendingMessage::ApplyTextInsertingCommands(std::string input, uint32_t escape_char, bool maniacs_parsing) {
	if (input.empty()) {
		return input;
	}

	std::string output;

	const char* iter = input.data();
	const auto end = input.data() + input.size();

	const char* start_copy = iter;
	while (iter != end) {
		auto ret = Utils::UTF8Next(iter, end);
		if (ret.ch != escape_char) {
			iter = ret.next;
			continue;
		}

		// utf8 parsing failed
		if (ret.ch == 0) {
			break;
		}

		output.append(start_copy, iter - start_copy);
		start_copy = iter;

		iter = ret.next;
		if (iter == end) {
			break;
		}

		const auto ch = *iter;
		++iter;

		if (ch == 'N' || ch == 'n') {
			auto parse_ret = Game_Message::ParseActor(iter, end, escape_char, true);
			iter = parse_ret.next;
			int value = parse_ret.value;

			const auto* actor = Main_Data::game_actors->GetActor(value);
			if (!actor) {
				Output::Warning("Invalid Actor Id {} in message text", value);
			} else{
				output.append(ToString(actor->GetName()));
			}

			start_copy = iter;
		} else if (ch == 'V' || ch == 'v') {
			auto parse_ret = Game_Message::ParseVariable(iter, end, escape_char, true);
			iter = parse_ret.next;
			int value = parse_ret.value;

			int variable_value = Main_Data::game_variables->Get(value);
			output.append(std::to_string(variable_value));

			start_copy = iter;
		} else if (ch == 'T' || ch == 't') {
			auto parse_ret = Game_Message::ParseString(iter, end, escape_char, true);
			iter = parse_ret.next;
			int value = parse_ret.value;

			Game_Strings::Str_t string = Main_Data::game_strings->Get(value);
			output.append((std::string)string);

			start_copy = iter;
		} else if (maniacs_parsing && (ch == 'S' || ch == 's')) {
			// parsing a switch within an extracted string var command will parse \s[N] as a switch (ON/OFF)
			auto parse_ret = Game_Message::ParseSpeed(iter, end, escape_char, true);
			iter = parse_ret.next;
			int value = parse_ret.value;

			bool sw = Main_Data::game_switches->Get(value);
			if (sw) output.append("ON");
			else output.append("OFF");
			
			start_copy = iter;
		}
	}

	if (start_copy == input.data()) {
		// Fast path - no substitutions occured, so just move the input into the return value.
		output = std::move(input);
	} else {
		output.append(start_copy, end - start_copy);
	}

	return output;
}


