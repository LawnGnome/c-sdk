package utilization

import (
	"fmt"
	"strings"
	"time"
)

// Helper constants, functions, and types common to multiple providers are
// contained in this file.

// Constants from the spec.
const (
	maxFieldValueSize = 255             // The maximum value size, in bytes.
	providerTimeout   = 1 * time.Second // The maximum time a HTTP provider may block.
)

// validationError represents a response from a provider endpoint that doesn't
// match the format expectations. The spec says we SHOULD increment the
// `Supportability/utilization/$provider/error` supportability metric when we
// get one of these. However, to create this metric we would have to store it
// in the utilization structure and add it to each application's harvest data.
// This was deemed more bother than it's worth, especially since we would need
// additional logic to only send this once per application.
type validationError struct{ e error }

func (a validationError) Error() string {
	return a.e.Error()
}

func isValidationError(e error) bool {
	_, is := e.(validationError)
	return is
}

// This function normalises string values per the utilisation spec.
func normalizeValue(s string) (string, error) {
	out := strings.TrimSpace(s)

	// The spec defines the length check in bytes, not codepoints, which seems a
	// bit odd for a UTF-8 string, but whatever.
	bytes := []byte(out)
	if len(bytes) > maxFieldValueSize {
		return "", validationError{fmt.Errorf("response is too long: got %d; expected <=%d", len(bytes), maxFieldValueSize)}
	}

	for i, r := range out {
		if !isAcceptableRune(r) {
			return "", validationError{fmt.Errorf("bad character %x at position %d in response", r, i)}
		}
	}

	return out, nil
}

func isAcceptableRune(r rune) bool {
	switch r {
	case 0xFFFD:
		return false // invalid UTF-8
	case '_', ' ', '/', '.', '-':
		return true
	default:
		return r > 0x7f || // still allows some invalid UTF-8, but that's the spec.
			('0' <= r && r <= '9') ||
			('a' <= r && r <= 'z') ||
			('A' <= r && r <= 'Z')
	}
}
