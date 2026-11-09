/* Copyright 2013-2021 MultiMC Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <QCollator>
#include <QSortFilterProxyModel>

class InstanceProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

   public:
    InstanceProxyModel(QObject* parent = 0);

    /// Obsidium's main window shows the developer's packs and the player's own on different pages.
    enum class Filter { All, Developer, User };
    void setFilter(Filter filter);
    Filter filter() const { return m_filter; }
    /// The packs shipped by the developer (the bundled PolyTech instance).
    static bool isDeveloperInstance(const QString& id);

   protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
    bool subSortLessThan(const QModelIndex& left, const QModelIndex& right) const;

   private:
    QCollator m_naturalSort;
    Filter m_filter = Filter::All;
};
