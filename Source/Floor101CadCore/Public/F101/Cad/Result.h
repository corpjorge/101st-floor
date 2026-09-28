#pragma once

#include "F101/Cad/Assert.h"

#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace f101
{
	enum class ErrorCode
	{
		InvalidArgument,
		OutOfRange,
		NotFound,
		ParseError,
	};

	struct Error
	{
		ErrorCode code = ErrorCode::InvalidArgument;
		std::string message;
	};

	// Outcome of an operation that can fail for expected reasons (user input, missing entities).
	// Exceptions are disabled in Unreal modules (ADR-0002), so failures are values.
	template <typename T>
	class [[nodiscard]] Result
	{
	public:
		Result(T value) : state(std::in_place_index<0>, std::move(value)) {}
		Result(Error error) : state(std::in_place_index<1>, std::move(error)) {}

		bool ok() const { return state.index() == 0; }
		explicit operator bool() const { return ok(); }

		const T& value() const&
		{
			F101_ASSERT(ok());
			return *std::get_if<0>(&state);
		}

		T&& value() &&
		{
			F101_ASSERT(ok());
			return std::move(*std::get_if<0>(&state));
		}

		const Error& error() const
		{
			F101_ASSERT(!ok());
			return *std::get_if<1>(&state);
		}

	private:
		std::variant<T, Error> state;
	};

	// Result of an operation that produces no value.
	class [[nodiscard]] Status
	{
	public:
		Status() = default;
		Status(Error error) : failure(std::move(error)) {}

		static Status success() { return Status(); }

		bool ok() const { return !failure.has_value(); }
		explicit operator bool() const { return ok(); }

		const Error& error() const
		{
			F101_ASSERT(!ok());
			return *failure;
		}

	private:
		std::optional<Error> failure;
	};
}
