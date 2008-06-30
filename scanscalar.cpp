#include "scanscalar.h"
#include "scanner.h"
#include "exp.h"
#include "exceptions.h"
#include "token.h"

namespace YAML
{
	// ScanScalar
	// . This is where the scalar magic happens.
	//
	// . We do the scanning in three phases:
	//   1. Scan until newline
	//   2. Eat newline
	//   3. Scan leading blanks.
	//
	// . Depending on the parameters given, we store or stop
	//   and different places in the above flow.
	std::string ScanScalar(Stream& INPUT, ScanScalarParams& params)
	{
		bool foundNonEmptyLine = false;
		bool emptyLine = false, moreIndented = false;
		std::string scalar;
		params.leadingSpaces = false;

		while(INPUT) {
			// ********************************
			// Phase #1: scan until line ending
			while(!params.end.Matches(INPUT) && !Exp::Break.Matches(INPUT)) {
				if(INPUT.peek() == EOF)
					break;

				// document indicator?
				if(INPUT.column == 0 && Exp::DocIndicator.Matches(INPUT)) {
					if(params.onDocIndicator == BREAK)
						break;
					else if(params.onDocIndicator == THROW)
						throw IllegalDocIndicator();
				}

				foundNonEmptyLine = true;

				// escaped newline? (only if we're escaping on slash)
				if(params.escape == '\\' && Exp::EscBreak.Matches(INPUT)) {
					int n = Exp::EscBreak.Match(INPUT);
					INPUT.Eat(n);
					continue;
				}

				// escape this?
				if(INPUT.peek() == params.escape) {
					scalar += Exp::Escape(INPUT);
					continue;
				}

				// otherwise, just add the damn character
				scalar += INPUT.GetChar();
			}

			// eof? if we're looking to eat something, then we throw
			if(INPUT.peek() == EOF) {
				if(params.eatEnd)
					throw IllegalEOF();
				break;
			}

			// doc indicator?
			if(params.onDocIndicator == BREAK && INPUT.column == 0 && Exp::DocIndicator.Matches(INPUT))
				break;

			// are we done via character match?
			int n = params.end.Match(INPUT);
			if(n >= 0) {
				if(params.eatEnd)
					INPUT.Eat(n);
				break;
			}

			// ********************************
			// Phase #2: eat line ending
			n = Exp::Break.Match(INPUT);
			INPUT.Eat(n);

			// ********************************
			// Phase #3: scan initial spaces

			// first the required indentation
			while(INPUT.peek() == ' ' && (INPUT.column < params.indent || (params.detectIndent && !foundNonEmptyLine)))
				INPUT.Eat(1);

			// update indent if we're auto-detecting
			if(params.detectIndent && !foundNonEmptyLine)
				params.indent = std::max(params.indent, INPUT.column);

			// and then the rest of the whitespace
			while(Exp::Blank.Matches(INPUT)) {
				// we check for tabs that masquerade as indentation
				if(INPUT.peek() == '\t'&& INPUT.column < params.indent && params.onTabInIndentation == THROW)
					throw IllegalTabInIndentation();

				if(!params.eatLeadingWhitespace)
					break;

				INPUT.Eat(1);
			}

			// was this an empty line?
			bool nextEmptyLine = Exp::Break.Matches(INPUT);
			bool nextMoreIndented = (INPUT.peek() == ' ');

			// TODO: for block scalars, we always start with a newline, so we should fold OR keep that

			if(params.fold && !emptyLine && !nextEmptyLine && !moreIndented && !nextMoreIndented)
				scalar += " ";
			else
				scalar += "\n";

			emptyLine = nextEmptyLine;
			moreIndented = nextMoreIndented;

			// are we done via indentation?
			if(!emptyLine && INPUT.column < params.indent) {
				params.leadingSpaces = true;
				break;
			}
		}

		// post-processing
		if(params.trimTrailingSpaces) {
			unsigned pos = scalar.find_last_not_of(' ');
			if(pos < scalar.size())
				scalar.erase(pos + 1);
		}

		if(params.chomp <= 0) {
			unsigned pos = scalar.find_last_not_of('\n');
			if(params.chomp == 0 && pos + 1 < scalar.size())
				scalar.erase(pos + 2);
			else if(params.chomp == -1 && pos < scalar.size())
				scalar.erase(pos + 1);
		}

		return scalar;
	}
}