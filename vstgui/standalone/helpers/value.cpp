#include "value.h"
#include "../ivaluelistener.h"
#include "../../lib/dispatchlist.h"

//------------------------------------------------------------------------
namespace VSTGUI {
namespace Standalone {
namespace Detail {
namespace /* anonymous */ {

//------------------------------------------------------------------------
IValue::Type convertStepToValue (IStepValue::StepType step, IStepValue::StepType steps)
{
	return static_cast<IValue::Type> (step) / static_cast<IValue::Type> (steps);
}

//------------------------------------------------------------------------
IStepValue::StepType convertValueToStep (IValue::Type value, IStepValue::StepType steps)
{
	return std::min (steps, static_cast<IStepValue::StepType> (value * static_cast<IValue::Type> (steps + 1)));
}

//------------------------------------------------------------------------
} // anonymous

//------------------------------------------------------------------------
class DefaultValueStringConverter : public IValueStringConverter
{
public:
	UTF8String valueAsString (IValue::Type value) const override
	{
		UTF8String result;
		if (value < 0. || value > 1.)
			return result;

		result = std::to_string (value);
		return result;
	}
	
	IValue::Type stringAsValue (const UTF8String& string) const override
	{
		IValue::Type value;
		std::istringstream sstream (string.get ());
		sstream.imbue (std::locale::classic ());
		sstream.precision (40);
		sstream >> value;
		if (value < 0. || value > 1.)
			return IValue::InvalidValue;
		return value;
	}
};

//------------------------------------------------------------------------
class StringListValueStringConverter : public IValueStringConverter
{
public:
	explicit StringListValueStringConverter (const std::initializer_list<UTF8String>& list) : strings (list)
	{
	}
	
	UTF8String valueAsString (IValue::Type value) const override
	{
		auto index =
		convertValueToStep (value, static_cast<IStepValue::StepType> (strings.size () - 1));
		return strings[index];
	}
	
	IValue::Type stringAsValue (const UTF8String& string) const override
	{
		auto it = std::find (strings.begin (), strings.end (), string);
		if (it != strings.end ())
			return convertStepToValue (static_cast<IStepValue::StepType> (std::distance (strings.begin (), it)),
									   static_cast<IStepValue::StepType> (strings.size () - 1));
			return IValue::InvalidValue;
	}
	
private:
	std::vector<UTF8String> strings;
};

//------------------------------------------------------------------------
class Value : public IValue
{
public:
	Value (const IdStringPtr id, Type initialValue, const ValueStringConverterPtr& stringConverter);

	void beginEdit () override;
	bool performEdit (Type newValue) override;
	void endEdit () override;
	
	void setActive (bool state) override;
	bool isActive () const override;
	
	Type getValue () const override;
	bool isEditing () const override;
	
	const UTF8String& getID () const override;

	const IValueStringConverter& getStringConverter () const override;
	
	void registerListener (IValueListener* listener) override;
	void unregisterListener (IValueListener* listener) override;

	bool hasStringConverter () const { return stringConverter != nullptr; }
private:
	UTF8String idString;
	Type value;
	bool active {true};
	uint32_t editCount {0};
	ValueStringConverterPtr stringConverter;
	DispatchList<IValueListener> listeners;
};

//------------------------------------------------------------------------
class StepValue : public Value, public IStepValue, public IValueStringConverter
{
public:
	StepValue (const IdStringPtr id, StepType initialSteps, Type initialValue, const ValueStringConverterPtr& stringConverter);

	bool performEdit (Type newValue) override;

	StepType getSteps () const override;
	IValue::Type stepToValue (StepType step) const override;
	StepType valueToStep (IValue::Type) const override;

	UTF8String valueAsString (IValue::Type value) const override;
	IValue::Type stringAsValue (const UTF8String& string) const override;

	const IValueStringConverter& getStringConverter () const override;
private:
	StepType steps;
};

//------------------------------------------------------------------------
Value::Value (const IdStringPtr id, Type initialValue, const ValueStringConverterPtr& stringConverter)
: idString (id)
, value (initialValue)
, stringConverter (stringConverter)
{
}

//------------------------------------------------------------------------
void Value::beginEdit ()
{
	++editCount;
	
	if (editCount == 1)
	{
		listeners.forEach ([this] (IValueListener* l) {
			l->onBeginEdit (*this);
		});
	}
}

//------------------------------------------------------------------------
bool Value::performEdit (Type newValue)
{
	if (newValue < 0. || newValue > 1.)
		return false;
	if (newValue == value)
		return true;
	value = newValue;
	
	listeners.forEach ([this] (IValueListener* l) {
		l->onPerformEdit (*this, value);
	});
	
	return true;
}

//------------------------------------------------------------------------
void Value::endEdit ()
{
	vstgui_assert (editCount > 0);
	--editCount;

	if (editCount == 0)
	{
		listeners.forEach ([this] (IValueListener* l) {
			l->onEndEdit (*this);
		});
	}
}

//------------------------------------------------------------------------
void Value::setActive (bool state)
{
	if (state == active)
		return;
	active = state;

	listeners.forEach ([this] (IValueListener* l) {
		l->onStateChange (*this);
	});
}

//------------------------------------------------------------------------
bool Value::isActive () const
{
	return active;
}

//------------------------------------------------------------------------
Value::Type Value::getValue () const
{
	return value;
}

//------------------------------------------------------------------------
bool Value::isEditing () const
{
	return editCount != 0;
}

//------------------------------------------------------------------------
const UTF8String& Value::getID () const
{
	return idString;
}

//------------------------------------------------------------------------
const IValueStringConverter& Value::getStringConverter () const
{
	return *stringConverter.get ();
}

//------------------------------------------------------------------------
void Value::registerListener (IValueListener* listener)
{
	listeners.add (listener);
}

//------------------------------------------------------------------------
void Value::unregisterListener (IValueListener* listener)
{
	listeners.remove (listener);
}

//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
StepValue::StepValue (const IdStringPtr id, StepType initialSteps, Type initialValue,
                      const ValueStringConverterPtr& stringConverter)
: Value (id, initialValue, stringConverter)
, steps (initialSteps - 1)
{
}

//------------------------------------------------------------------------
bool StepValue::performEdit (Type newValue)
{
	return Value::performEdit (stepToValue (valueToStep (newValue)));
}

//------------------------------------------------------------------------
StepValue::StepType StepValue::getSteps () const
{
	return steps + 1;
}

//------------------------------------------------------------------------
IValue::Type StepValue::stepToValue (StepType step) const
{
	return convertStepToValue(step, steps);
}

//------------------------------------------------------------------------
StepValue::StepType StepValue::valueToStep (IValue::Type value) const
{
	return convertValueToStep (value, steps);
}

//------------------------------------------------------------------------
UTF8String StepValue::valueAsString (IValue::Type value) const
{
	auto v = valueToStep (value);
	return UTF8String (std::to_string (v));
}

//------------------------------------------------------------------------
IValue::Type StepValue::stringAsValue (const UTF8String& string) const
{
	StepType v;
	std::istringstream sstream (string.get ());
	sstream.imbue (std::locale::classic ());
	sstream >> v;
	if (v > steps)
		return IValue::InvalidValue;
	return stepToValue (v);
}

//------------------------------------------------------------------------
const IValueStringConverter& StepValue::getStringConverter () const
{
	if (!hasStringConverter ())
		return *this;
	return Value::getStringConverter ();
}

//------------------------------------------------------------------------
} // Detail

//------------------------------------------------------------------------
namespace Value {

//------------------------------------------------------------------------
ValuePtr make (const UTF8String& id, IValue::Type initialValue,
               const ValueStringConverterPtr& stringConverter)
{
	vstgui_assert (id.empty () == false);
	return std::make_shared<Detail::Value> (
	    id, initialValue, stringConverter.get () ?
	                          stringConverter :
	                          std::make_shared<Detail::DefaultValueStringConverter> ());
}

//------------------------------------------------------------------------
ValuePtr makeStepValue (const UTF8String& id, IStepValue::StepType initialSteps,
                        IValue::Type initialValue, const ValueStringConverterPtr& stringConverter)
{
	vstgui_assert (id.empty () == false);
	return std::make_shared<Detail::StepValue> (id, initialSteps, initialValue, stringConverter);
}

//------------------------------------------------------------------------
ValuePtr makeStringListValue (const UTF8String& id,
                              const std::initializer_list<UTF8String>& strings)
{
	vstgui_assert (id.empty () == false);
	return std::make_shared<Detail::StepValue> (
	    id, static_cast<IStepValue::StepType> (strings.size ()), 0,
	    std::make_shared<Detail::StringListValueStringConverter> (strings));
}

//------------------------------------------------------------------------
} // Value
} // Standalone
} // VSTGUI
