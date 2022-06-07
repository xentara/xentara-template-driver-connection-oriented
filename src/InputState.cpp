// Copyright (c) embedded ocean GmbH
#include "InputState.hpp"

#include "Attributes.hpp"

#include <xentara/memory/WriteSentinel.hpp>

namespace xentara::plugins::templateDriver
{

template <std::regular DataType>
auto InputState<DataType>::resolveAttribute(std::u16string_view name) -> const model::Attribute *
{
	// Check all the attributes we support
	return model::Attribute::resolve(name,
		model::Attribute::kUpdateTime,
		kValueAttribute,
		model::Attribute::kChangeTime,
		model::Attribute::kQuality,
		attributes::kError);
}

template <std::regular DataType>
auto InputState<DataType>::resolveEvent(std::u16string_view name, std::shared_ptr<void> parent) -> std::shared_ptr<process::Event>
{
	// Check all the events we support
	if (name == model::Attribute::kValue)
	{
		return std::shared_ptr<process::Event>(parent, &_valueChangedEvent);
	}
	else if (name == model::Attribute::kQuality)
	{
		return std::shared_ptr<process::Event>(parent, &_qualityChangedEvent);
	}
	else if (name == process::Event::kChanged)
	{
		return std::shared_ptr<process::Event>(parent, &_changedEvent);
	}

	// The event name is not known
	return nullptr;
}

template <std::regular DataType>
auto InputState<DataType>::readHandle(const model::Attribute &attribute) const noexcept -> std::optional<data::ReadHandle>
{
	// Try reach readable attribute
	if (attribute == model::Attribute::kUpdateTime)
	{
		return _dataBlock.member(&State::_updateTime);
	}
	else if (attribute == kValueAttribute)
	{
		return _dataBlock.member(&State::_value);
	}
	else if (attribute == model::Attribute::kChangeTime)
	{
		return _dataBlock.member(&State::_changeTime);
	}
	else if (attribute == model::Attribute::kQuality)
	{
		return _dataBlock.member(&State::_quality);
	}
	else if (attribute == attributes::kError)
	{
		return _dataBlock.member(&State::_error);
	}

	return data::ReadHandle::Error::Unknown;
}

template <std::regular DataType>
auto InputState<DataType>::valueReadHandle() const noexcept -> data::ReadHandle
{
	return _dataBlock.member(&State::_value);
}

template <std::regular DataType>
auto InputState<DataType>::prepare() -> void
{
	// Create the data block
	_dataBlock.create(memory::memoryResources::data());
}

template <std::regular DataType>
auto InputState<DataType>::update(std::chrono::system_clock::time_point time, const DataType &value, std::error_code error) -> void
{
	// Make a write sentinel
	memory::WriteSentinel sentinel { _dataBlock };
	auto &state = *sentinel;
	const auto &oldState = sentinel.oldValue();

	// Update the state
	state._updateTime = context.scheduledTime();
	state._value = value;
	state._quality = error ? data::Quality::Bad : data::Quality::Good;
	state._error = attributes::errorCode(error);

	// Detect changes
	const auto valueChanged = state._value != oldState._value;
	const auto qualityChanged = state._quality != oldState._quality;
	const auto errorChanged = state._error != oldState._error;
	const bool changed = valueChanged | qualityChanged | errorChanged;

	// Update the change time, if necessary. We always need to write the change time, even if it is the same as before,
	// because the memory resource might use swap-in.
	state._changeTime = changed ? context.scheduledTime() : oldState._changeTime;

	// Commit the data before sending the events
	sentinel.commit();

	// Fire the correct events
	if (valueChanged)
	{
		_valueChangedEvent.fire();
	}
	if (qualityChanged)
	{
		_qualityChangedEvent.fire();
	}
	if (changed)
	{
		_changedEvent.fire();
	}
}

// TODO: add template instantiations for other supported types
template class InputState<double>;

} // namespace xentara::plugins::templateDriver