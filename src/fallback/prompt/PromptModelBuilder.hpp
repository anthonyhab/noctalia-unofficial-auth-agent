#pragma once

#include "PromptModel.hpp"

#include <QJsonObject>

namespace bb::fallback::prompt {

    class PromptModelBuilder {
      public:
        PromptDisplayModel build(const QJsonObject& event) const;
    };

} // namespace bb::fallback::prompt
