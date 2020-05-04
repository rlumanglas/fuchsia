// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package codegen

const implementationTemplate = `
{{- define "Implementation" -}}
// WARNING: This file is machine generated by fidlgen.

#include <{{ .PrimaryHeader }}>

#include "lib/fidl/cpp/internal/implementation.h"

{{- range .Library }}
namespace {{ . }} {
{{- end }}
{{ "" }}

{{- range .Decls }}
{{- if Eq .Kind Kinds.Const }}{{ template "ConstDefinition" . }}{{- end }}
{{- if Eq .Kind Kinds.Protocol }}{{ template "DispatchProtocolDefinition" . }}{{- end }}
{{- if Eq .Kind Kinds.Struct }}{{ template "StructDefinition" . }}{{- end }}
{{- if Eq .Kind Kinds.Union }}{{ template "UnionDefinition" . }}{{- end }}
{{- if Eq .Kind Kinds.Table }}{{ template "TableDefinition" . }}{{- end }}
{{- end }}

{{- range .LibraryReversed }}
}  // namespace {{ . }}
{{- end }}

{{ end }}

{{- define "DispatchProtocolDefinition" -}}
{{- range $transport, $_ := .Transports }}
{{- if eq $transport "Channel" -}}{{ template "ProtocolDefinition" $ }}{{- end }}
{{- end }}
{{- end -}}
`
