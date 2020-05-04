// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package files

const Source = `
{{- define "Source" -}}
// WARNING: This file is machine generated by fidlgen.

#include <{{ .PrimaryHeader }}>
#include <memory>
{{ "" }}

{{- "" }}
namespace llcpp {
{{ range .Library }}
namespace {{ . }} {
{{- end }}

{{- range .Decls }}
{{- if Eq .Kind Kinds.Const }}{{ template "ConstDefinition" . }}{{- end }}
{{- if Eq .Kind Kinds.Protocol }}{{ template "ProtocolDefinition" . }}{{- end }}
{{- if Eq .Kind Kinds.Struct }}{{ template "StructDefinition" . }}{{- end }}
{{- if Eq .Kind Kinds.Union }}{{ template "UnionDefinition" . }}{{- end }}
{{- if Eq .Kind Kinds.Table }}{{ template "TableDefinition" . }}{{- end }}
{{- end }}
{{ "" }}

{{- range .LibraryReversed }}
}  // namespace {{ . }}
{{- end }}
}  // namespace llcpp
{{ end }}
`
